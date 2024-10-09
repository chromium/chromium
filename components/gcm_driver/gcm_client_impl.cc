// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_client_impl.h"

#include <stddef.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "components/crx_file/id_util.h"
#include "components/gcm_driver/crypto/gcm_decryption_result.h"
#include "components/gcm_driver/features.h"
#include "components/gcm_driver/gcm_account_mapper.h"
#include "components/gcm_driver/gcm_backoff_policy.h"
#include "google_apis/gcm/base/encryptor.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/engine/checkin_request.h"
#include "google_apis/gcm/engine/connection_factory_impl.h"
#include "google_apis/gcm/engine/gcm_registration_request_handler.h"
#include "google_apis/gcm/engine/gcm_store_impl.h"
#include "google_apis/gcm/engine/gcm_unregistration_request_handler.h"
#include "google_apis/gcm/engine/instance_id_delete_token_request_handler.h"
#include "google_apis/gcm/engine/instance_id_get_token_request_handler.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace gcm {

// It is okay to append to the enum if these states grow. DO NOT reorder,
// renumber or otherwise reuse existing values.
// Do not assign an explicit value to REGISTRATION_CACHE_STATUS_COUNT, as
// this lets the compiler keep it up to date.
enum class RegistrationCacheStatus {
  REGISTRATION_NOT_FOUND = 0,
  REGISTRATION_FOUND_AND_FRESH = 1,
  REGISTRATION_FOUND_BUT_STALE = 2,
  REGISTRATION_FOUND_BUT_SENDERS_DONT_MATCH = 3,
  // NOTE: always keep this entry at the end. Add new value only immediately
  // above this line. Make sure to update the corresponding histogram enum
  // accordingly.
  REGISTRATION_CACHE_STATUS_COUNT
};

namespace {

// Indicates a message type of the received message.
enum MessageType {
  UNKNOWN,           // Undetermined type.
  DATA_MESSAGE,      // Regular data message.
  DELETED_MESSAGES,  // Messages were deleted on the server.
  SEND_ERROR,        // Error sending a message.
};

const int kMaxRegistrationRetries = 5;
const int kMaxUnregistrationRetries = 5;
const char kDeletedCountKey[] = "total_deleted";
const char kMessageTypeDataMessage[] = "gcm";
const char kMessageTypeDeletedMessagesKey[] = "deleted_messages";
const char kMessageTypeKey[] = "message_type";
const char kMessageTypeSendErrorKey[] = "send_error";
const char kSubtypeKey[] = "subtype";
const char kSendMessageFromValue[] = "gcm@chrome.com";
const int64_t kDefaultUserSerialNumber = 0LL;
const int kDestroyGCMStoreDelayMS = 5 * 60 * 1000;  // 5 minutes.

GCMClient::Result ToGCMClientResult(MCSClient::MessageSendStatus status) {
  switch (status) {
    case MCSClient::QUEUED:
      return GCMClient::SUCCESS;
    case MCSClient::MESSAGE_TOO_LARGE:
      return GCMClient::INVALID_PARAMETER;
    case MCSClient::QUEUE_SIZE_LIMIT_REACHED:
    case MCSClient::APP_QUEUE_SIZE_LIMIT_REACHED:
    case MCSClient::NO_CONNECTION_ON_ZERO_TTL:
    case MCSClient::TTL_EXCEEDED:
      return GCMClient::NETWORK_ERROR;
    case MCSClient::SENT:
    case MCSClient::SEND_STATUS_COUNT:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return GCMClientImpl::UNKNOWN_ERROR;
}

void ToCheckinProtoVersion(
    const GCMClient::ChromeBuildInfo& chrome_build_info,
    checkin_proto::ChromeBuildProto* android_build_info) {
  checkin_proto::ChromeBuildProto_Platform platform =
      checkin_proto::ChromeBuildProto_Platform_PLATFORM_LINUX;
  switch (chrome_build_info.platform) {
    case GCMClient::PLATFORM_WIN:
      platform = checkin_proto::ChromeBuildProto_Platform_PLATFORM_WIN;
      break;
    case GCMClient::PLATFORM_MAC:
      platform = checkin_proto::ChromeBuildProto_Platform_PLATFORM_MAC;
      break;
    case GCMClient::PLATFORM_LINUX:
      platform = checkin_proto::ChromeBuildProto_Platform_PLATFORM_LINUX;
      break;
    case GCMClient::PLATFORM_IOS:
      platform = checkin_proto::ChromeBuildProto_Platform_PLATFORM_IOS;
      break;
    case GCMClient::PLATFORM_ANDROID:
      platform = checkin_proto::ChromeBuildProto_Platform_PLATFORM_ANDROID;
      break;
    case GCMClient::PLATFORM_CROS:
      platform = checkin_proto::ChromeBuildProto_Platform_PLATFORM_CROS;
      break;
    case GCMClient::PLATFORM_UNSPECIFIED:
      // For unknown platform, return as LINUX.
      platform = checkin_proto::ChromeBuildProto_Platform_PLATFORM_LINUX;
      break;
  }
  android_build_info->set_platform(platform);

  checkin_proto::ChromeBuildProto_Channel channel =
      checkin_proto::ChromeBuildProto_Channel_CHANNEL_UNKNOWN;
  switch (chrome_build_info.channel) {
    case GCMClient::CHANNEL_STABLE:
      channel = checkin_proto::ChromeBuildProto_Channel_CHANNEL_STABLE;
      break;
    case GCMClient::CHANNEL_BETA:
      channel = checkin_proto::ChromeBuildProto_Channel_CHANNEL_BETA;
      break;
    case GCMClient::CHANNEL_DEV:
      channel = checkin_proto::ChromeBuildProto_Channel_CHANNEL_DEV;
      break;
    case GCMClient::CHANNEL_CANARY:
      channel = checkin_proto::ChromeBuildProto_Channel_CHANNEL_CANARY;
      break;
    case GCMClient::CHANNEL_UNKNOWN:
      channel = checkin_proto::ChromeBuildProto_Channel_CHANNEL_UNKNOWN;
      break;
  }
  android_build_info->set_channel(channel);

  android_build_info->set_chrome_version(chrome_build_info.version);
}

MessageType DecodeMessageType(const std::string& value) {
  if (kMessageTypeDeletedMessagesKey == value)
    return DELETED_MESSAGES;
  if (kMessageTypeSendErrorKey == value)
    return SEND_ERROR;
  if (kMessageTypeDataMessage == value)
    return DATA_MESSAGE;
  return UNKNOWN;
}

int ConstructGCMVersion(const std::string& chrome_version) {
  // Major Chrome version is passed as GCM version.
  auto parts = base::SplitStringOnce(chrome_version, '.');
  if (!parts) {
    NOTREACHED_IN_MIGRATION();
    return 0;
  }

  int gcm_version = 0;
  base::StringToInt(parts->first, &gcm_version);
  return gcm_version;
}

std::string SerializeInstanceIDData(const std::string& instance_id,
                                    const std::string& extra_data) {
  DCHECK(!instance_id.empty() && !extra_data.empty());
  DCHECK(instance_id.find(',') == std::string::npos);
  return instance_id + "," + extra_data;
}

bool DeserializeInstanceIDData(const std::string& serialized_data,
                               std::string* instance_id,
                               std::string* extra_data) {
  DCHECK(instance_id && extra_data);
  auto parts = base::SplitStringOnce(serialized_data, ',');
  if (!parts) {
    return false;
  }
  *instance_id = parts->first;
  *extra_data = parts->second;
  return !instance_id->empty() && !extra_data->empty();
}

bool InstanceIDUsesSubtypeForAppId(const std::string& app_id) {
  // Always use subtypes with Instance ID, except for Chrome Apps/Extensions.
  return !crx_file::id_util::IdIsValid(app_id);
}

}  // namespace

void RecordRegistrationRequestToUMA(gcm::RegistrationCacheStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "GCM.RegistrationCacheStatus", status,
      RegistrationCacheStatus::REGISTRATION_CACHE_STATUS_COUNT);
}
GCMInternalsBuilder::GCMInternalsBuilder() = default;
GCMInternalsBuilder::~GCMInternalsBuilder() = default;

base::Clock* GCMInternalsBuilder::GetClock() {
  return base::DefaultClock::GetInstance();
}

std::unique_ptr<MCSClient> GCMInternalsBuilder::BuildMCSClient(
    const std::string& version,
    base::Clock* clock,
    ConnectionFactory* connection_factory,
    GCMStore* gcm_store,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    GCMStatsRecorder* recorder) {
  return std::make_unique<MCSClient>(version, clock, connection_factory,
                                     gcm_store, std::move(io_task_runner),
                                     recorder);
}

std::unique_ptr<ConnectionFactory> GCMInternalsBuilder::BuildConnectionFactory(
    const std::vector<GURL>& endpoints,
    const net::BackoffEntry::Policy& backoff_policy,
    base::RepeatingCallback<void(
        mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
        get_socket_factory_callback,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    GCMStatsRecorder* recorder,
    network::NetworkConnectionTracker* network_connection_tracker) {
  return std::make_unique<ConnectionFactoryImpl>(
      endpoints, backoff_policy, std::move(get_socket_factory_callback),
      std::move(io_task_runner), recorder, network_connection_tracker);
}

GCMClientImpl::CheckinInfo::CheckinInfo()
    : android_id(0), secret(0), accounts_set(false) {}

GCMClientImpl::CheckinInfo::~CheckinInfo() = default;

void GCMClientImpl::CheckinInfo::SnapshotCheckinAccounts() {
  last_checkin_accounts.clear();
  for (auto iter = account_tokens.begin(); iter != account_tokens.end();
       ++iter) {
    last_checkin_accounts.insert(iter->first);
  }
}

void GCMClientImpl::CheckinInfo::Reset() {
  android_id = 0;
  secret = 0;
  accounts_set = false;
  account_tokens.clear();
  last_checkin_accounts.clear();
}

GCMClientImpl::GCMClientImpl(
    std::unique_ptr<GCMInternalsBuilder> internals_builder)
    : internals_builder_(std::move(internals_builder)),
      state_(UNINITIALIZED),
      delegate_(nullptr),
      start_mode_(DELAYED_START),
      clock_(internals_builder_->GetClock()),
      gcm_store_reset_(false),
      network_connection_tracker_(nullptr) {}

GCMClientImpl::~GCMClientImpl() = default;

void GCMClientImpl::Initialize(
    const ChromeBuildInfo& chrome_build_info,
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    base::RepeatingCallback<void(
        mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
        get_socket_factory_callback,
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    std::unique_ptr<Encryptor> encryptor,
    GCMClient::Delegate* delegate) {
  DCHECK_EQ(UNINITIALIZED, state_);
  DCHECK(delegate);
  DCHECK(io_task_runner);
  DCHECK(io_task_runner->RunsTasksInCurrentSequence());

  get_socket_factory_callback_ = std::move(get_socket_factory_callback);
  url_loader_factory_ = url_loader_factory;
  network_connection_tracker_ = network_connection_tracker;
  chrome_build_info_ = chrome_build_info;
  gcm_store_ = std::make_unique<GCMStoreImpl>(path, blocking_task_runner,
                                              std::move(encryptor));
  delegate_ = delegate;
  io_task_runner_ = std::move(io_task_runner);
  recorder_.SetDelegate(this);
  state_ = INITIALIZED;
}

void GCMClientImpl::Start(StartMode start_mode) {
  DCHECK_NE(UNINITIALIZED, state_);
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  if (state_ == LOADED) {
    // Start the GCM if not yet.
    if (start_mode == IMMEDIATE_START) {
      // Give up the scheduling to wipe out the store since now some one starts
      // to use GCM.
      destroying_gcm_store_ptr_factory_.InvalidateWeakPtrs();

      StartGCM();
    }
    return;
  }

  // The delay start behavior will be abandoned when Start has been called
  // once with IMMEDIATE_START behavior.
  if (start_mode == IMMEDIATE_START)
    start_mode_ = IMMEDIATE_START;

  // Bail out if the loading is not started or completed.
  if (state_ != INITIALIZED)
    return;

  // Once the loading is completed, the check-in will be initiated.
  // If we're in lazy start mode, don't create a new store since none is really
  // using GCM functionality yet.
  gcm_store_->Load((start_mode == IMMEDIATE_START) ? GCMStore::CREATE_IF_MISSING
                                                   : GCMStore::DO_NOT_CREATE,
                   base::BindOnce(&GCMClientImpl::OnLoadCompleted,
                                  weak_ptr_factory_.GetWeakPtr()));
  state_ = LOADING;
}

void GCMClientImpl::OnLoadCompleted(
    std::unique_ptr<GCMStore::LoadResult> result) {
  DCHECK_EQ(LOADING, state_);
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  if (!result->success) {
    if (result->store_does_not_exist) {
      if (start_mode_ == IMMEDIATE_START) {
        // An immediate start was requested during the delayed start that just
        // completed. Perform it now.
        gcm_store_->Load(GCMStore::CREATE_IF_MISSING,
                         base::BindOnce(&GCMClientImpl::OnLoadCompleted,
                                        weak_ptr_factory_.GetWeakPtr()));
      } else {
        // In the case that the store does not exist, set |state_| back to
        // INITIALIZED such that store loading could be triggered again when
        // Start() is called with IMMEDIATE_START.
        state_ = INITIALIZED;
      }
    } else {
      // Otherwise, destroy the store to try again.
      ResetStore();
    }
    return;
  }
  gcm_store_reset_ = false;

  device_checkin_info_.android_id = result->device_android_id;
  device_checkin_info_.secret = result->device_security_token;
  device_checkin_info_.last_checkin_accounts = result->last_checkin_accounts;
  // A case where there were previously no accounts reported with checkin is
  // considered to be the same as when the list of accounts is empty. It enables
  // scheduling a periodic checkin for devices with no signed in users
  // immediately after restart, while keeping |accounts_set == false| delays the
  // checkin until the list of accounts is set explicitly.
  if (result->last_checkin_accounts.size() == 0)
    device_checkin_info_.accounts_set = true;
  last_checkin_time_ = result->last_checkin_time;
  gservices_settings_.UpdateFromLoadResult(*result);

  for (const auto& [key, value] : result->registrations) {
    std::string registration_id;
    scoped_refptr<RegistrationInfo> registration =
        RegistrationInfo::BuildFromString(key, value, &registration_id);
    // TODO(jianli): Add UMA to track the error case.
    if (registration)
      registrations_.emplace(std::move(registration), registration_id);
  }

  for (const auto& [key, value] : result->instance_id_data) {
    std::string instance_id;
    std::string extra_data;
    if (DeserializeInstanceIDData(value, &instance_id, &extra_data))
      instance_id_data_[key] = std::make_pair(instance_id, extra_data);
  }

  load_result_ = std::move(result);
  state_ = LOADED;

  // Don't initiate the GCM connection when GCM is in delayed start mode and
  // not any standalone app has registered GCM yet.
  if (start_mode_ == DELAYED_START && !HasStandaloneRegisteredApp()) {
    // If no standalone app is using GCM and the device ID is present, schedule
    // to have the store wiped out.
    if (device_checkin_info_.android_id) {
      DVLOG(1) << "GCM is in delayed start mode and there is no standalone "
                  "app, posting task to wipe store in "
               << base::Milliseconds(kDestroyGCMStoreDelayMS)
               << " milliseconds.";
      io_task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&GCMClientImpl::DestroyStoreWhenNotNeeded,
                         destroying_gcm_store_ptr_factory_.GetWeakPtr()),
          base::Milliseconds(kDestroyGCMStoreDelayMS));
    }

    return;
  }

  StartGCM();
}

void GCMClientImpl::StartGCM() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  // Taking over the value of account_mappings before passing the ownership of
  // load result to InitializeMCSClient.
  std::vector<AccountMapping> account_mappings;
  account_mappings.swap(load_result_->account_mappings);
  base::Time last_token_fetch_time = load_result_->last_token_fetch_time;

  InitializeMCSClient();

  if (device_checkin_info_.IsValid()) {
    SchedulePeriodicCheckin();
    OnReady(account_mappings, last_token_fetch_time);
    return;
  }

  state_ = INITIAL_DEVICE_CHECKIN;
  device_checkin_info_.Reset();

  DVLOG(1) << "Starting initial GCM checkin.";
  StartCheckin();
}

void GCMClientImpl::InitializeMCSClient() {
  DCHECK(network_connection_tracker_);
  std::vector<GURL> endpoints;
  endpoints.push_back(gservices_settings_.GetMCSMainEndpoint());
  GURL fallback_endpoint = gservices_settings_.GetMCSFallbackEndpoint();
  if (fallback_endpoint.is_valid())
    endpoints.push_back(fallback_endpoint);
  connection_factory_ = internals_builder_->BuildConnectionFactory(
      endpoints, GetGCMBackoffPolicy(), get_socket_factory_callback_,
      io_task_runner_, &recorder_, network_connection_tracker_);
  connection_factory_->SetConnectionListener(this);
  mcs_client_ = internals_builder_->BuildMCSClient(
      chrome_build_info_.version, clock_, connection_factory_.get(),
      gcm_store_.get(), io_task_runner_, &recorder_);

  mcs_client_->Initialize(
      base::BindRepeating(&GCMClientImpl::OnMCSError,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&GCMClientImpl::OnMessageReceivedFromMCS,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&GCMClientImpl::OnMessageSentToMCS,
                          weak_ptr_factory_.GetWeakPtr()),
      std::move(load_result_));
}

void GCMClientImpl::OnFirstTimeDeviceCheckinCompleted(
    const CheckinInfo& checkin_info) {
  DCHECK(!device_checkin_info_.IsValid());

  device_checkin_info_.android_id = checkin_info.android_id;
  device_checkin_info_.secret = checkin_info.secret;
  // If accounts were not set by now, we can consider them set (to empty list)
  // to make sure periodic checkins get scheduled after initial checkin.
  device_checkin_info_.accounts_set = true;
  gcm_store_->SetDeviceCredentials(
      checkin_info.android_id, checkin_info.secret,
      base::BindOnce(&GCMClientImpl::SetDeviceCredentialsCallback,
                     weak_ptr_factory_.GetWeakPtr()));

  OnReady(std::vector<AccountMapping>(), base::Time());
}

void GCMClientImpl::OnReady(const std::vector<AccountMapping>& account_mappings,
                            const base::Time& last_token_fetch_time) {
  state_ = READY;
  StartMCSLogin();

  delegate_->OnGCMReady(account_mappings, last_token_fetch_time);
}

void GCMClientImpl::StartMCSLogin() {
  DCHECK_EQ(READY, state_);
  DCHECK(device_checkin_info_.IsValid());
  mcs_client_->Login(device_checkin_info_.android_id,
                     device_checkin_info_.secret);
}

void GCMClientImpl::DestroyStoreWhenNotNeeded() {
  if (state_ != LOADED || start_mode_ != DELAYED_START)
    return;

  gcm_store_->Destroy(base::BindOnce(&GCMClientImpl::DestroyStoreCallback,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void GCMClientImpl::ResetStore() {
  // If already being reset, don't do it again. We want to prevent from
  // resetting and loading from the store again and again.
  if (gcm_store_reset_) {
    state_ = UNINITIALIZED;
    return;
  }
  gcm_store_reset_ = true;

  // Destroy the GCM store to start over.
  gcm_store_->Destroy(base::BindOnce(&GCMClientImpl::ResetStoreCallback,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void GCMClientImpl::SetAccountTokens(
    const std::vector<AccountTokenInfo>& account_tokens) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  device_checkin_info_.account_tokens.clear();
  for (auto iter = account_tokens.begin(); iter != account_tokens.end();
       ++iter) {
    device_checkin_info_.account_tokens[iter->email] = iter->access_token;
  }

  bool accounts_set_before = device_checkin_info_.accounts_set;
  device_checkin_info_.accounts_set = true;

  DVLOG(1) << "Set account called with: " << account_tokens.size()
           << " accounts.";

  if (state_ != READY && state_ != INITIAL_DEVICE_CHECKIN)
    return;

  bool account_removed = false;
  for (auto iter = device_checkin_info_.last_checkin_accounts.begin();
       iter != device_checkin_info_.last_checkin_accounts.end(); ++iter) {
    if (device_checkin_info_.account_tokens.find(*iter) ==
        device_checkin_info_.account_tokens.end()) {
      account_removed = true;
    }
  }

  // Checkin will be forced when any of the accounts was removed during the
  // current Chrome session or if there has been an account removed between the
  // restarts of Chrome. If there is a checkin in progress, it will be canceled.
  // We only force checkin when user signs out. When there is a new account
  // signed in, the periodic checkin will take care of adding the association in
  // reasonable time.
  if (account_removed) {
    DVLOG(1) << "Detected that account has been removed. Forcing checkin.";
    checkin_request_.reset();
    StartCheckin();
  } else if (!accounts_set_before) {
    SchedulePeriodicCheckin();
    DVLOG(1) << "Accounts set for the first time. Scheduled periodic checkin.";
  }
}

void GCMClientImpl::UpdateAccountMapping(
    const AccountMapping& account_mapping) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  gcm_store_->AddAccountMapping(
      account_mapping, base::BindOnce(&GCMClientImpl::DefaultStoreCallback,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void GCMClientImpl::RemoveAccountMapping(const CoreAccountId& account_id) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  gcm_store_->RemoveAccountMapping(
      account_id, base::BindOnce(&GCMClientImpl::DefaultStoreCallback,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void GCMClientImpl::SetLastTokenFetchTime(const base::Time& time) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  gcm_store_->SetLastTokenFetchTime(
      time,
      base::BindOnce(&GCMClientImpl::IgnoreWriteResultCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*operation_suffix_for_uma=*/"SetLastTokenFetchTime"));
}

void GCMClientImpl::UpdateHeartbeatTimer(
    std::unique_ptr<base::RetainingOneShotTimer> timer) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(mcs_client_);
  mcs_client_->UpdateHeartbeatTimer(std::move(timer));
}

void GCMClientImpl::AddInstanceIDData(const std::string& app_id,
                                      const std::string& instance_id,
                                      const std::string& extra_data) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  instance_id_data_[app_id] = std::make_pair(instance_id, extra_data);
  // TODO(crbug.com/40109289): If this call fails, we likely leak a registration
  // (the one stored in instance_id_data_ would be used for a registration but
  // not persisted).
  gcm_store_->AddInstanceIDData(
      app_id, SerializeInstanceIDData(instance_id, extra_data),
      base::BindOnce(&GCMClientImpl::IgnoreWriteResultCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*operation_suffix_for_uma=*/"AddInstanceIDData"));
}

void GCMClientImpl::RemoveInstanceIDData(const std::string& app_id) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  instance_id_data_.erase(app_id);
  gcm_store_->RemoveInstanceIDData(
      app_id,
      base::BindOnce(&GCMClientImpl::IgnoreWriteResultCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*operation_suffix_for_uma=*/"RemoveInstanceIDData"));
}

void GCMClientImpl::GetInstanceIDData(const std::string& app_id,
                                      std::string* instance_id,
                                      std::string* extra_data) {
  DCHECK(instance_id && extra_data);

  auto iter = instance_id_data_.find(app_id);
  if (iter == instance_id_data_.end())
    return;
  *instance_id = iter->second.first;
  *extra_data = iter->second.second;
}

void GCMClientImpl::AddHeartbeatInterval(const std::string& scope,
                                         int interval_ms) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(mcs_client_);
  mcs_client_->AddHeartbeatInterval(scope, interval_ms);
}

void GCMClientImpl::RemoveHeartbeatInterval(const std::string& scope) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(mcs_client_);
  mcs_client_->RemoveHeartbeatInterval(scope);
}

void GCMClientImpl::StartCheckin() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  // Make sure no checkin is in progress.
  if (checkin_request_)
    return;

  checkin_proto::ChromeBuildProto chrome_build_proto;
  ToCheckinProtoVersion(chrome_build_info_, &chrome_build_proto);

  std::map<std::string, std::string> empty_account_tokens;

  CheckinRequest::RequestInfo request_info(
      device_checkin_info_.android_id, device_checkin_info_.secret,
      empty_account_tokens, gservices_settings_.digest(), chrome_build_proto);
  checkin_request_ = std::make_unique<CheckinRequest>(
      gservices_settings_.GetCheckinURL(), request_info, GetGCMBackoffPolicy(),
      base::BindOnce(&GCMClientImpl::OnCheckinCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      url_loader_factory_, io_task_runner_, &recorder_);
  // Taking a snapshot of the accounts count here, as there might be an asynch
  // update of the account tokens while checkin is in progress.
  device_checkin_info_.SnapshotCheckinAccounts();
  checkin_request_->Start();
}

void GCMClientImpl::OnCheckinCompleted(
    net::HttpStatusCode response_code,
    const checkin_proto::AndroidCheckinResponse& checkin_response) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  checkin_request_.reset();

  if (response_code == net::HTTP_UNAUTHORIZED ||
      response_code == net::HTTP_BAD_REQUEST) {
    LOG(ERROR) << "Checkin rejected. Resetting GCM Store.";
    ResetStore();
    return;
  }

  DCHECK(checkin_response.has_android_id());
  DCHECK(checkin_response.has_security_token());
  CheckinInfo checkin_info;
  checkin_info.android_id = checkin_response.android_id();
  checkin_info.secret = checkin_response.security_token();

  if (state_ == INITIAL_DEVICE_CHECKIN) {
    OnFirstTimeDeviceCheckinCompleted(checkin_info);
  } else {
    // checkin_info is not expected to change after a periodic checkin as it
    // would invalidate the registration IDs.
    DCHECK_EQ(READY, state_);
    DCHECK_EQ(device_checkin_info_.android_id, checkin_info.android_id);
    DCHECK_EQ(device_checkin_info_.secret, checkin_info.secret);
  }

  if (device_checkin_info_.IsValid()) {
    // First update G-services settings, as something might have changed.
    if (gservices_settings_.UpdateFromCheckinResponse(checkin_response)) {
      gcm_store_->SetGServicesSettings(
          gservices_settings_.settings_map(), gservices_settings_.digest(),
          base::BindOnce(&GCMClientImpl::SetGServicesSettingsCallback,
                         weak_ptr_factory_.GetWeakPtr()));
    }

    last_checkin_time_ = clock_->Now();
    gcm_store_->SetLastCheckinInfo(
        last_checkin_time_, device_checkin_info_.last_checkin_accounts,
        base::BindOnce(&GCMClientImpl::SetLastCheckinInfoCallback,
                       weak_ptr_factory_.GetWeakPtr()));
    SchedulePeriodicCheckin();
  }
}

void GCMClientImpl::SetGServicesSettingsCallback(bool success) {
  DCHECK(success);
}

void GCMClientImpl::SchedulePeriodicCheckin() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  // Make sure no checkin is in progress.
  if (checkin_request_.get() || !device_checkin_info_.accounts_set)
    return;

  // There should be only one periodic checkin pending at a time. Removing
  // pending periodic checkin to schedule a new one.
  periodic_checkin_ptr_factory_.InvalidateWeakPtrs();

  base::TimeDelta time_to_next_checkin = GetTimeToNextCheckin();
  if (time_to_next_checkin.is_negative())
    time_to_next_checkin = base::TimeDelta();

  io_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GCMClientImpl::StartCheckin,
                     periodic_checkin_ptr_factory_.GetWeakPtr()),
      time_to_next_checkin);
}

base::TimeDelta GCMClientImpl::GetTimeToNextCheckin() const {
  return last_checkin_time_ + gservices_settings_.GetCheckinInterval() -
         clock_->Now();
}

void GCMClientImpl::SetLastCheckinInfoCallback(bool success) {
  // TODO(fgorski): This is one of the signals that store needs a rebuild.
  DCHECK(success);
}

void GCMClientImpl::SetDeviceCredentialsCallback(bool success) {
  // TODO(fgorski): This is one of the signals that store needs a rebuild.
  DCHECK(success);
}

void GCMClientImpl::UpdateRegistrationCallback(bool success) {
  // TODO(fgorski): This is one of the signals that store needs a rebuild.
  DCHECK(success);
}

void GCMClientImpl::DefaultStoreCallback(bool success) {
  DCHECK(success);
}

void GCMClientImpl::IgnoreWriteResultCallback(
    const std::string& operation_suffix_for_uma,
    bool success) {
  // TODO(crbug.com/40691191): Implement proper error handling.
  // TODO(fgorski): Ignoring the write result for now to make sure
  // sync_intergration_tests are not broken.
}

void GCMClientImpl::DestroyStoreCallback(bool success) {
  ResetCache();

  if (!success) {
    LOG(ERROR) << "Failed to destroy GCM store";
    state_ = UNINITIALIZED;
    return;
  }

  state_ = INITIALIZED;
}

void GCMClientImpl::ResetStoreCallback(bool success) {
  // Even an incomplete reset may invalidate registrations, and this might be
  // the only opportunity to notify the delegate. For example a partial reset
  // that deletes the "CURRENT" file will cause GCMStoreImpl to consider the DB
  // to no longer exist, in which case the next load will simply create a new
  // store rather than resetting it.
  delegate_->OnStoreReset();

  if (!success) {
    LOG(ERROR) << "Failed to reset GCM store";
    state_ = UNINITIALIZED;
    return;
  }

  state_ = INITIALIZED;
  Start(start_mode_);
}

void GCMClientImpl::Stop() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  // TODO(fgorski): Perhaps we should make a distinction between a Stop and a
  // Shutdown.
  DVLOG(1) << "Stopping the GCM Client";
  ResetCache();
  state_ = INITIALIZED;
  gcm_store_->Close();
}

void GCMClientImpl::ResetCache() {
  DVLOG(2) << "Resetting GCMClientImpl cache";

  weak_ptr_factory_.InvalidateWeakPtrs();
  periodic_checkin_ptr_factory_.InvalidateWeakPtrs();
  device_checkin_info_.Reset();
  connection_factory_.reset();
  delegate_->OnDisconnected();
  mcs_client_.reset();
  checkin_request_.reset();
  // Delete all of the pending registration and unregistration requests.
  pending_registration_requests_.clear();
  pending_unregistration_requests_.clear();
}

void GCMClientImpl::Register(
    scoped_refptr<RegistrationInfo> registration_info) {
  DCHECK_EQ(state_, READY);
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  // Registrations should never pass as an app_id the special category used
  // internally when registering with a subtype. See security note in
  // GCMClientImpl::HandleIncomingMessage.
  CHECK_NE(registration_info->app_id,
           chrome_build_info_.product_category_for_subtypes);

  // Find and use the cached registration ID.
  RegistrationInfoMap::const_iterator registrations_iter =
      registrations_.find(registration_info);
  if (registrations_iter != registrations_.end()) {
    bool matched = true;

    // For GCM registration, we also match the sender IDs since multiple
    // registrations are not supported.
    const GCMRegistrationInfo* gcm_registration_info =
        GCMRegistrationInfo::FromRegistrationInfo(registration_info.get());
    if (gcm_registration_info) {
      const GCMRegistrationInfo* cached_gcm_registration_info =
          GCMRegistrationInfo::FromRegistrationInfo(
              registrations_iter->first.get());
      DCHECK(cached_gcm_registration_info);
      if (gcm_registration_info->sender_ids !=
          cached_gcm_registration_info->sender_ids) {
        matched = false;
        // Senders IDs don't match existing registration.
        RecordRegistrationRequestToUMA(
            RegistrationCacheStatus::REGISTRATION_FOUND_BUT_SENDERS_DONT_MATCH);
      }
    }

    if (matched) {
      // Skip registration if token is fresh.
      base::TimeDelta token_invalidation_period =
          features::GetTokenInvalidationInterval();
      base::TimeDelta time_since_last_validation =
          clock_->Now() - registrations_iter->first->last_validated;
      if (token_invalidation_period.is_zero() ||
          time_since_last_validation < token_invalidation_period) {
        // Token is fresh, or token invalidation is disabled.
        // Use cached registration.
        delegate_->OnRegisterFinished(registration_info,
                                      registrations_iter->second, SUCCESS);
        RecordRegistrationRequestToUMA(
            RegistrationCacheStatus::REGISTRATION_FOUND_AND_FRESH);
        return;
      } else {
        // Token is stale.
        RecordRegistrationRequestToUMA(
            RegistrationCacheStatus::REGISTRATION_FOUND_BUT_STALE);
      }
    }
  } else {
    // New Registration request (no existing registration)
    RecordRegistrationRequestToUMA(
        RegistrationCacheStatus::REGISTRATION_NOT_FOUND);
  }

  std::unique_ptr<RegistrationRequest::CustomRequestHandler> request_handler;
  std::string source_to_record;

  const GCMRegistrationInfo* gcm_registration_info =
      GCMRegistrationInfo::FromRegistrationInfo(registration_info.get());
  if (gcm_registration_info) {
    std::string senders;
    for (const auto& kv : gcm_registration_info->sender_ids) {
      if (!senders.empty())
        senders.append(",");
      senders.append(kv);
    }
    request_handler = std::make_unique<GCMRegistrationRequestHandler>(senders);
    source_to_record = senders;
  }

  const InstanceIDTokenInfo* instance_id_token_info =
      InstanceIDTokenInfo::FromRegistrationInfo(registration_info.get());
  if (instance_id_token_info) {
    auto instance_id_iter = instance_id_data_.find(registration_info->app_id);
    CHECK(instance_id_iter != instance_id_data_.end(),
          base::NotFatalUntil::M130);

    request_handler = std::make_unique<InstanceIDGetTokenRequestHandler>(
        instance_id_iter->second.first,
        instance_id_token_info->authorized_entity,
        instance_id_token_info->scope,
        ConstructGCMVersion(chrome_build_info_.version),
        instance_id_token_info->time_to_live);
    source_to_record = instance_id_token_info->authorized_entity + "/" +
                       instance_id_token_info->scope;
  }

  bool use_subtype = instance_id_token_info &&
                     InstanceIDUsesSubtypeForAppId(registration_info->app_id);
  std::string category = use_subtype
                             ? chrome_build_info_.product_category_for_subtypes
                             : registration_info->app_id;
  std::string subtype = use_subtype ? registration_info->app_id : std::string();
  RegistrationRequest::RequestInfo request_info(device_checkin_info_.android_id,
                                                device_checkin_info_.secret,
                                                category, subtype);

  std::unique_ptr<RegistrationRequest> registration_request(
      new RegistrationRequest(
          gservices_settings_.GetRegistrationURL(), request_info,
          std::move(request_handler), GetGCMBackoffPolicy(),
          base::BindOnce(&GCMClientImpl::OnRegisterCompleted,
                         weak_ptr_factory_.GetWeakPtr(), registration_info),
          kMaxRegistrationRetries, url_loader_factory_, io_task_runner_,
          &recorder_, source_to_record));
  registration_request->Start();
  pending_registration_requests_.insert(
      std::make_pair(registration_info, std::move(registration_request)));
}

void GCMClientImpl::OnRegisterCompleted(
    scoped_refptr<RegistrationInfo> registration_info,
    RegistrationRequest::Status status,
    const std::string& registration_id) {
  DCHECK(delegate_);

  Result result;
  PendingRegistrationRequests::const_iterator iter =
      pending_registration_requests_.find(registration_info);
  if (iter == pending_registration_requests_.end()) {
    result = UNKNOWN_ERROR;
  } else if (status == RegistrationRequest::INVALID_SENDER) {
    result = INVALID_PARAMETER;
  } else if (registration_id.empty()) {
    // All other errors are currently treated as SERVER_ERROR (including
    // REACHED_MAX_RETRIES due to the device being offline!).
    result = SERVER_ERROR;
  } else {
    result = SUCCESS;
  }

  if (result == SUCCESS) {
    // Cache it.
    // Note that the existing cached record has to be removed first because
    // otherwise the key value in registrations_ will not be updated. For GCM
    // registrations, the key consists of pair of app_id and sender_ids though
    // only app_id is used in the key comparison.
    registrations_.erase(registration_info);
    registration_info->last_validated = clock_->Now();
    registrations_[registration_info] = registration_id;

    // Save it in the persistent store.
    gcm_store_->AddRegistration(
        registration_info->GetSerializedKey(),
        registration_info->GetSerializedValue(registration_id),
        base::BindOnce(&GCMClientImpl::UpdateRegistrationCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  delegate_->OnRegisterFinished(
      registration_info, result == SUCCESS ? registration_id : std::string(),
      result);

  if (iter != pending_registration_requests_.end())
    pending_registration_requests_.erase(iter);
}

bool GCMClientImpl::ValidateRegistration(
    scoped_refptr<RegistrationInfo> registration_info,
    const std::string& registration_id) {
  DCHECK_EQ(state_, READY);
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  // Must have a cached registration.
  RegistrationInfoMap::const_iterator registrations_iter =
      registrations_.find(registration_info);
  if (registrations_iter == registrations_.end())
    return false;

  // Cached registration ID must match.
  const std::string& cached_registration_id = registrations_iter->second;
  if (registration_id != cached_registration_id)
    return false;

  // For GCM registration, we also match the sender IDs since multiple
  // registrations are not supported.
  const GCMRegistrationInfo* gcm_registration_info =
      GCMRegistrationInfo::FromRegistrationInfo(registration_info.get());
  if (gcm_registration_info) {
    const GCMRegistrationInfo* cached_gcm_registration_info =
        GCMRegistrationInfo::FromRegistrationInfo(
            registrations_iter->first.get());
    DCHECK(cached_gcm_registration_info);
    if (gcm_registration_info->sender_ids !=
        cached_gcm_registration_info->sender_ids) {
      return false;
    }
  }

  return true;
}

void GCMClientImpl::Unregister(
    scoped_refptr<RegistrationInfo> registration_info) {
  DCHECK_EQ(state_, READY);
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  std::unique_ptr<UnregistrationRequest::CustomRequestHandler> request_handler;
  std::string source_to_record;

  const GCMRegistrationInfo* gcm_registration_info =
      GCMRegistrationInfo::FromRegistrationInfo(registration_info.get());
  if (gcm_registration_info) {
    request_handler = std::make_unique<GCMUnregistrationRequestHandler>(
        registration_info->app_id);
  }

  const InstanceIDTokenInfo* instance_id_token_info =
      InstanceIDTokenInfo::FromRegistrationInfo(registration_info.get());
  if (instance_id_token_info) {
    auto instance_id_iter = instance_id_data_.find(registration_info->app_id);
    if (instance_id_iter == instance_id_data_.end()) {
      // This should not be reached since we should not delete tokens when
      // an InstanceID has not been created yet.
      NOTREACHED_IN_MIGRATION();
      return;
    }

    request_handler = std::make_unique<InstanceIDDeleteTokenRequestHandler>(
        instance_id_iter->second.first,
        instance_id_token_info->authorized_entity,
        instance_id_token_info->scope,
        ConstructGCMVersion(chrome_build_info_.version));
    source_to_record = instance_id_token_info->authorized_entity + "/" +
                       instance_id_token_info->scope;
  }

  // Remove the registration/token(s) from the cache and the store.
  // TODO(jianli): Remove it only when the request is successful.
  if (instance_id_token_info &&
      instance_id_token_info->authorized_entity == "*" &&
      instance_id_token_info->scope == "*") {
    // If authorized_entity and scope are '*', find and remove all associated
    // tokens.
    bool token_found = false;
    for (auto iter = registrations_.begin(); iter != registrations_.end();) {
      InstanceIDTokenInfo* cached_instance_id_token_info =
          InstanceIDTokenInfo::FromRegistrationInfo(iter->first.get());
      if (cached_instance_id_token_info &&
          cached_instance_id_token_info->app_id == registration_info->app_id) {
        token_found = true;
        gcm_store_->RemoveRegistration(
            cached_instance_id_token_info->GetSerializedKey(),
            base::BindOnce(&GCMClientImpl::UpdateRegistrationCallback,
                           weak_ptr_factory_.GetWeakPtr()));
        registrations_.erase(iter++);
      } else {
        ++iter;
      }
    }

    // If no token is found for the Instance ID, don't need to unregister
    // since the Instance ID is not sent to the server yet.
    if (!token_found) {
      OnUnregisterCompleted(registration_info, UnregistrationRequest::SUCCESS);
      return;
    }
  } else {
    auto iter = registrations_.find(registration_info);
    if (iter == registrations_.end()) {
      delegate_->OnUnregisterFinished(registration_info, INVALID_PARAMETER);
      return;
    }
    registrations_.erase(iter);

    gcm_store_->RemoveRegistration(
        registration_info->GetSerializedKey(),
        base::BindOnce(&GCMClientImpl::UpdateRegistrationCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  bool use_subtype = instance_id_token_info &&
                     InstanceIDUsesSubtypeForAppId(registration_info->app_id);
  std::string category = use_subtype
                             ? chrome_build_info_.product_category_for_subtypes
                             : registration_info->app_id;
  std::string subtype = use_subtype ? registration_info->app_id : std::string();
  UnregistrationRequest::RequestInfo request_info(
      device_checkin_info_.android_id, device_checkin_info_.secret, category,
      subtype);

  std::unique_ptr<UnregistrationRequest> unregistration_request(
      new UnregistrationRequest(
          gservices_settings_.GetRegistrationURL(), request_info,
          std::move(request_handler), GetGCMBackoffPolicy(),
          base::BindOnce(&GCMClientImpl::OnUnregisterCompleted,
                         weak_ptr_factory_.GetWeakPtr(), registration_info),
          kMaxUnregistrationRetries, url_loader_factory_, io_task_runner_,
          &recorder_, source_to_record));
  unregistration_request->Start();
  pending_unregistration_requests_.insert(
      std::make_pair(registration_info, std::move(unregistration_request)));
}

void GCMClientImpl::OnUnregisterCompleted(
    scoped_refptr<RegistrationInfo> registration_info,
    UnregistrationRequest::Status status) {
  DVLOG(1) << "Unregister completed for app: " << registration_info->app_id
           << " with " << (status ? "success." : "failure.");

  Result result;
  switch (status) {
    case UnregistrationRequest::SUCCESS:
      result = SUCCESS;
      break;
    case UnregistrationRequest::INVALID_PARAMETERS:
      result = INVALID_PARAMETER;
      break;
    default:
      // All other errors are currently treated as SERVER_ERROR (including
      // REACHED_MAX_RETRIES due to the device being offline!).
      result = SERVER_ERROR;
      break;
  }
  delegate_->OnUnregisterFinished(registration_info, result);

  pending_unregistration_requests_.erase(registration_info);
}

void GCMClientImpl::OnGCMStoreDestroyed(bool success) {
  DLOG_IF(ERROR, !success) << "GCM store failed to be destroyed!";
}

void GCMClientImpl::Send(const std::string& app_id,
                         const std::string& receiver_id,
                         const OutgoingMessage& message) {
  DCHECK_EQ(state_, READY);
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  mcs_proto::DataMessageStanza stanza;
  stanza.set_ttl(message.time_to_live);
  stanza.set_sent(clock_->Now().ToInternalValue() /
                  base::Time::kMicrosecondsPerSecond);
  stanza.set_id(message.id);
  stanza.set_from(kSendMessageFromValue);
  stanza.set_to(receiver_id);
  stanza.set_category(app_id);

  for (auto iter = message.data.begin(); iter != message.data.end(); ++iter) {
    mcs_proto::AppData* app_data = stanza.add_app_data();
    app_data->set_key(iter->first);
    app_data->set_value(iter->second);
  }

  MCSMessage mcs_message(stanza);
  DVLOG(1) << "MCS message size: " << mcs_message.size();
  mcs_client_->SendMessage(mcs_message);
}

std::string GCMClientImpl::GetStateString() const {
  switch (state_) {
    case GCMClientImpl::UNINITIALIZED:
      return "UNINITIALIZED";
    case GCMClientImpl::INITIALIZED:
      return "INITIALIZED";
    case GCMClientImpl::LOADING:
      return "LOADING";
    case GCMClientImpl::LOADED:
      return "LOADED";
    case GCMClientImpl::INITIAL_DEVICE_CHECKIN:
      return "INITIAL_DEVICE_CHECKIN";
    case GCMClientImpl::READY:
      return "READY";
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

void GCMClientImpl::RecordDecryptionFailure(const std::string& app_id,
                                            GCMDecryptionResult result) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  recorder_.RecordDecryptionFailure(app_id, result);
}

void GCMClientImpl::SetRecording(bool recording) {
  recorder_.set_is_recording(recording);
}

void GCMClientImpl::ClearActivityLogs() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  recorder_.Clear();
}

GCMClient::GCMStatistics GCMClientImpl::GetStatistics() const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  GCMClient::GCMStatistics stats;
  stats.gcm_client_created = true;
  stats.is_recording = recorder_.is_recording();
  stats.gcm_client_state = GetStateString();
  stats.connection_client_created = mcs_client_ != nullptr;
  stats.last_checkin = last_checkin_time_;
  stats.next_checkin =
      last_checkin_time_ + gservices_settings_.GetCheckinInterval();
  if (connection_factory_)
    stats.connection_state = connection_factory_->GetConnectionStateString();
  if (mcs_client_) {
    stats.send_queue_size = mcs_client_->GetSendQueueSize();
    stats.resend_queue_size = mcs_client_->GetResendQueueSize();
  }
  if (device_checkin_info_.android_id > 0)
    stats.android_id = device_checkin_info_.android_id;
  if (device_checkin_info_.secret > 0)
    stats.android_secret = device_checkin_info_.secret;

  recorder_.CollectActivities(&stats.recorded_activities);

  for (auto it = registrations_.begin(); it != registrations_.end(); ++it) {
    stats.registered_app_ids.push_back(it->first->app_id);
  }
  return stats;
}

void GCMClientImpl::OnActivityRecorded() {
  delegate_->OnActivityRecorded();
}

void GCMClientImpl::OnConnected(const GURL& current_server,
                                const net::IPEndPoint& ip_endpoint) {
  // TODO(gcm): expose current server in debug page.
  delegate_->OnActivityRecorded();
  delegate_->OnConnected(ip_endpoint);
}

void GCMClientImpl::OnDisconnected() {
  delegate_->OnActivityRecorded();
  delegate_->OnDisconnected();
}

void GCMClientImpl::OnMessageReceivedFromMCS(const gcm::MCSMessage& message) {
  switch (message.tag()) {
    case kLoginResponseTag:
      DVLOG(1) << "Login response received by GCM Client. Ignoring.";
      return;
    case kDataMessageStanzaTag:
      DVLOG(1) << "A downstream message received. Processing...";
      HandleIncomingMessage(message);
      return;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Message with unexpected tag received by GCMClient";
      return;
  }
}

void GCMClientImpl::OnMessageSentToMCS(int64_t user_serial_number,
                                       const std::string& app_id,
                                       const std::string& message_id,
                                       MCSClient::MessageSendStatus status) {
  DCHECK_EQ(user_serial_number, kDefaultUserSerialNumber);
  DCHECK(delegate_);

  // TTL_EXCEEDED is singled out here, because it can happen long time after the
  // message was sent. That is why it comes as |OnMessageSendError| event rather
  // than |OnSendFinished|. SendErrorDetails.additional_data is left empty.
  // All other errors will be raised immediately, through asynchronous callback.
  // It is expected that TTL_EXCEEDED will be issued for a message that was
  // previously issued |OnSendFinished| with status SUCCESS.
  // TODO(jianli): Consider adding UMA for this status.
  if (status == MCSClient::TTL_EXCEEDED) {
    SendErrorDetails send_error_details;
    send_error_details.message_id = message_id;
    send_error_details.result = GCMClient::TTL_EXCEEDED;
    delegate_->OnMessageSendError(app_id, send_error_details);
  } else if (status == MCSClient::SENT) {
    delegate_->OnSendAcknowledged(app_id, message_id);
  } else {
    delegate_->OnSendFinished(app_id, message_id, ToGCMClientResult(status));
  }
}

void GCMClientImpl::OnMCSError() {
  // TODO(fgorski): For now it replaces the initialization method. Long term it
  // should have an error or status passed in.
}

void GCMClientImpl::HandleIncomingMessage(const gcm::MCSMessage& message) {
  DCHECK(delegate_);

  const mcs_proto::DataMessageStanza& data_message_stanza =
      reinterpret_cast<const mcs_proto::DataMessageStanza&>(
          message.GetProtobuf());
  DCHECK_EQ(data_message_stanza.device_user_id(), kDefaultUserSerialNumber);

  // Copy all the data from the stanza to a MessageData object. When present,
  // keys like kSubtypeKey or kMessageTypeKey will be filtered out later.
  MessageData message_data;
  for (int i = 0; i < data_message_stanza.app_data_size(); ++i) {
    std::string key = data_message_stanza.app_data(i).key();
    message_data[key] = data_message_stanza.app_data(i).value();
  }

  std::string subtype;
  auto subtype_iter = message_data.find(kSubtypeKey);
  if (subtype_iter != message_data.end()) {
    subtype = subtype_iter->second;
    message_data.erase(subtype_iter);
  }

  // SECURITY NOTE: Subtypes received from GCM *cannot* be trusted for
  // registrations without a subtype (as the sender can pass any subtype they
  // want). They can however be trusted for registrations that are known to have
  // a subtype (as GCM overwrites anything passed by the sender).
  //
  // So a given Chrome profile always passes a fixed string called
  // |product_category_for_subtypes| (of the form "com.chrome.macosx") as the
  // category when registering with a subtype, and incoming subtypes are only
  // trusted for that category.
  //
  // TODO(johnme): Remove this check if GCM starts sending the subtype in a
  // field that's guaranteed to be trusted (b/18198485).
  //
  // (On Android, all registrations made by Chrome on behalf of third-party
  // apps/extensions/websites have always had a subtype, so such a check is not
  // necessary - or possible, since category is fixed to the true package name).
  bool subtype_is_trusted = data_message_stanza.category() ==
                            chrome_build_info_.product_category_for_subtypes;
  bool use_subtype = subtype_is_trusted && !subtype.empty();
  std::string app_id = use_subtype ? subtype : data_message_stanza.category();

  MessageType message_type = DATA_MESSAGE;
  auto type_iter = message_data.find(kMessageTypeKey);
  if (type_iter != message_data.end()) {
    message_type = DecodeMessageType(type_iter->second);
    message_data.erase(type_iter);
  }

  switch (message_type) {
    case DATA_MESSAGE:
      HandleIncomingDataMessage(app_id, use_subtype, data_message_stanza,
                                message_data);
      break;
    case DELETED_MESSAGES:
      HandleIncomingDeletedMessages(app_id, data_message_stanza, message_data);
      break;
    case SEND_ERROR:
      HandleIncomingSendError(app_id, data_message_stanza, message_data);
      break;
    case UNKNOWN:
      DVLOG(1) << "Unknown message_type received. Message ignored. "
               << "App ID: " << app_id << ".";
      break;
  }
}

void GCMClientImpl::HandleIncomingDataMessage(
    const std::string& app_id,
    bool was_subtype,
    const mcs_proto::DataMessageStanza& data_message_stanza,
    MessageData& message_data) {
  recorder_.RecordDataMessageReceived(app_id, data_message_stanza.from(),
                                      data_message_stanza.ByteSize(),
                                      GCMStatsRecorder::DATA_MESSAGE);

  IncomingMessage incoming_message;
  incoming_message.sender_id = data_message_stanza.from();
  incoming_message.message_id = data_message_stanza.persistent_id();
  if (data_message_stanza.has_token())
    incoming_message.collapse_key = data_message_stanza.token();
  incoming_message.data = message_data;
  incoming_message.raw_data = data_message_stanza.raw_data();

  delegate_->OnMessageReceived(app_id, incoming_message);
}

void GCMClientImpl::HandleIncomingDeletedMessages(
    const std::string& app_id,
    const mcs_proto::DataMessageStanza& data_message_stanza,
    MessageData& message_data) {
  int deleted_count = 0;
  auto count_iter = message_data.find(kDeletedCountKey);
  if (count_iter != message_data.end()) {
    if (!base::StringToInt(count_iter->second, &deleted_count))
      deleted_count = 0;
  }

  recorder_.RecordDataMessageReceived(app_id, data_message_stanza.from(),
                                      data_message_stanza.ByteSize(),
                                      GCMStatsRecorder::DELETED_MESSAGES);
  delegate_->OnMessagesDeleted(app_id);
}

void GCMClientImpl::HandleIncomingSendError(
    const std::string& app_id,
    const mcs_proto::DataMessageStanza& data_message_stanza,
    MessageData& message_data) {
  SendErrorDetails send_error_details;
  send_error_details.additional_data = message_data;
  send_error_details.result = SERVER_ERROR;
  send_error_details.message_id = data_message_stanza.persistent_id();

  recorder_.RecordIncomingSendError(app_id, data_message_stanza.to(),
                                    data_message_stanza.id());
  delegate_->OnMessageSendError(app_id, send_error_details);
}

bool GCMClientImpl::HasStandaloneRegisteredApp() const {
  if (registrations_.empty())
    return false;
  // Note that account mapper is not counted as a standalone app since it is
  // automatically started when other app uses GCM.
  return registrations_.size() > 1 ||
         !ExistsGCMRegistrationInMap(registrations_, kGCMAccountMapperAppId);
}

}  // namespace gcm
