// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_client_impl.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "components/fcm/base/fcm_backoff_policy.h"
#include "components/fcm/crypto/fcm_decryption_result.h"
#include "components/gcm_driver/gcm_registration_manager_impl.h"
#include "google_apis/gcm/base/encryptor.h"
#include "google_apis/gcm/base/gcm_features.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/engine/checkin_request.h"
#include "google_apis/gcm/engine/connection_factory_impl.h"
#include "google_apis/gcm/engine/gcm_registration_request_handler.h"
#include "google_apis/gcm/engine/gcm_store_impl.h"
#include "google_apis/gcm/engine/gcm_unregistration_request_handler.h"
#include "google_apis/gcm/engine/instance_id_delete_token_request_handler.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace gcm {

namespace {

// Indicates a message type of the received message.
enum MessageType {
  UNKNOWN,           // Undetermined type.
  DATA_MESSAGE,      // Regular data message.
  DELETED_MESSAGES,  // Messages were deleted on the server.
  SEND_ERROR,        // Error sending a message.
};

const int kMaxUnregistrationRetries = 5;
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
      NOTREACHED();
  }
  return GCMClientImpl::UNKNOWN_ERROR;
}

MessageType DecodeMessageType(const std::string& value) {
  if (kMessageTypeDeletedMessagesKey == value) {
    return DELETED_MESSAGES;
  }
  if (kMessageTypeSendErrorKey == value) {
    return SEND_ERROR;
  }
  if (kMessageTypeDataMessage == value) {
    return DATA_MESSAGE;
  }
  return UNKNOWN;
}

}  // namespace

GCMClientImpl::GCMClientImpl(
    std::unique_ptr<fcm::FcmInternalsBuilder> internals_builder)
    : internals_builder_(std::move(internals_builder)),
      state_(UNINITIALIZED),
      start_mode_(DELAYED_START),
      clock_(internals_builder_->GetClock()),
      gcm_store_reset_(false),
      network_connection_tracker_(nullptr) {}

GCMClientImpl::~GCMClientImpl() = default;

void GCMClientImpl::Initialize(
    const fcm::ChromeBuildInfo& chrome_build_info,
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    base::RepeatingCallback<void(
        mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
        get_socket_factory_callback,
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    std::unique_ptr<Encryptor> encryptor) {
  DCHECK_EQ(UNINITIALIZED, state_);
  DCHECK(io_task_runner);
  DCHECK(io_task_runner->RunsTasksInCurrentSequence());

  get_socket_factory_callback_ = std::move(get_socket_factory_callback);
  url_loader_factory_ = url_loader_factory;
  network_connection_tracker_ = network_connection_tracker;
  chrome_build_info_ = chrome_build_info;
  gcm_store_ = std::make_unique<GCMStoreImpl>(path, blocking_task_runner,
                                              std::move(encryptor));
  registration_manager_ = std::make_unique<GCMRegistrationManagerImpl>(
      *gcm_store_, *clock_, chrome_build_info, url_loader_factory,
      io_task_runner, recorder_);
  io_task_runner_ = std::move(io_task_runner);
  recorder_.SetDelegate(this);
  state_ = INITIALIZED;
}

void GCMClientImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GCMClientImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void GCMClientImpl::Start(StartMode start_mode) {
  DCHECK_NE(UNINITIALIZED, state_);
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  // The delay start behavior will be abandoned when Start has been called
  // once with IMMEDIATE_START behavior.
  if (start_mode == IMMEDIATE_START) {
    start_mode_ = IMMEDIATE_START;
  }

  if (state_ == LOADED) {
    // Start the GCM if not yet.
    if (start_mode_ == IMMEDIATE_START) {
      // Give up the scheduling to wipe out the store since now some one starts
      // to use GCM.
      destroying_gcm_store_ptr_factory_.InvalidateWeakPtrs();

      StartGCM();
    }
    return;
  }

  // Bail out if the loading is not started or completed.
  if (state_ != INITIALIZED) {
    return;
  }

  // Once the loading is completed, the check-in will be initiated.
  // If we're in lazy start mode, don't create a new store since none is really
  // using GCM functionality yet.
  gcm_store_->Load((start_mode_ == IMMEDIATE_START)
                       ? GCMStore::CREATE_IF_MISSING
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

  device_checkin_info_.set_android_id(result->device_android_id);
  device_checkin_info_.set_secret(result->device_security_token);
  last_checkin_time_ = result->last_checkin_time;
  gservices_settings_.UpdateFromLoadResult(*result);

  CHECK(registration_manager_);
  registration_manager_->SetDeviceCheckinInfo(device_checkin_info_);
  registration_manager_->SetGServicesSettings(gservices_settings_);
  registration_manager_->OnRegistrationDataLoaded(*result);

  load_result_ = std::move(result);
  state_ = LOADED;

  // Don't initiate the GCM connection when GCM is in delayed start mode and
  // not any standalone app has registered GCM yet.
  if (start_mode_ == DELAYED_START && !HasStandaloneRegisteredApp()) {
    // If no standalone app is using GCM and the device ID is present, schedule
    // to have the store wiped out.
    if (device_checkin_info_.android_id()) {
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

  InitializeMCSClient();

  if (device_checkin_info_.IsValid()) {
    SchedulePeriodicCheckin();
    OnReady();
    return;
  }

  state_ = INITIAL_DEVICE_CHECKIN;
  device_checkin_info_.Reset();
  CHECK(registration_manager_);
  registration_manager_->SetDeviceCheckinInfo(device_checkin_info_);

  DVLOG(1) << "Starting initial GCM checkin.";
  StartCheckin();
}

void GCMClientImpl::InitializeMCSClient() {
  DCHECK(network_connection_tracker_);
  std::vector<GURL> endpoints;
  endpoints.push_back(gservices_settings_.GetMCSMainEndpoint());
  GURL fallback_endpoint = gservices_settings_.GetMCSFallbackEndpoint();
  if (fallback_endpoint.is_valid()) {
    endpoints.push_back(fallback_endpoint);
  }
  connection_factory_ = internals_builder_->BuildConnectionFactory(
      endpoints, fcm::GetBackoffPolicy(), get_socket_factory_callback_,
      io_task_runner_, &recorder_, network_connection_tracker_);
  connection_factory_->SetConnectionListener(this);
  mcs_client_ = internals_builder_->BuildMCSClient(
      chrome_build_info_.version, clock_, connection_factory_.get(),
      gcm_store_.get(), io_task_runner_, &recorder_);

  mcs_client_->Initialize(/*delegate=*/this, std::move(load_result_));
}

void GCMClientImpl::OnFirstTimeDeviceCheckinCompleted(
    const fcm::CheckinInfo& checkin_info) {
  DCHECK(!device_checkin_info_.IsValid());

  device_checkin_info_.set_android_id(checkin_info.android_id());
  device_checkin_info_.set_secret(checkin_info.secret());
  CHECK(registration_manager_);
  registration_manager_->SetDeviceCheckinInfo(device_checkin_info_);
  gcm_store_->SetDeviceCredentials(
      checkin_info.android_id(), checkin_info.secret(),
      base::BindOnce(&GCMClientImpl::SetDeviceCredentialsCallback,
                     weak_ptr_factory_.GetWeakPtr()));

  OnReady();
}

void GCMClientImpl::OnReady() {
  state_ = READY;
  StartMCSLogin();

  observers_.Notify(&Observer::OnGCMReady);
}

void GCMClientImpl::StartMCSLogin() {
  DCHECK_EQ(READY, state_);
  DCHECK(device_checkin_info_.IsValid());
  mcs_client_->Login(device_checkin_info_.android_id(),
                     device_checkin_info_.secret());
}

void GCMClientImpl::DestroyStoreWhenNotNeeded() {
  if (state_ != LOADED || start_mode_ != DELAYED_START) {
    return;
  }

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

void GCMClientImpl::AddInstanceIDData(const std::string& app_id,
                                      const std::string& instance_id,
                                      const std::string& extra_data) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  CHECK(registration_manager_);
  registration_manager_->AddInstanceIDData(app_id, instance_id, extra_data);
}

void GCMClientImpl::RemoveInstanceIDData(const std::string& app_id) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  CHECK(registration_manager_);
  registration_manager_->RemoveInstanceIDData(app_id);
}

void GCMClientImpl::GetInstanceIDData(const std::string& app_id,
                                      std::string* instance_id,
                                      std::string* extra_data) {
  CHECK(instance_id);
  CHECK(extra_data);
  CHECK(registration_manager_);
  std::tie(*instance_id, *extra_data) =
      registration_manager_->GetInstanceIDData(app_id);
}

void GCMClientImpl::GetToken(scoped_refptr<InstanceIDTokenInfo> token_info,
                             GetTokenCallback callback) {
  // TODO(crbug.com/589587357): Upgrade to `CHECK_EQ` once
  // `GCMDriverDesktop::OnStoreReset()` re-gates `delayed_task_controller_`.
  DCHECK_EQ(state_, READY);
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  CHECK(registration_manager_);
  registration_manager_->GetToken(std::move(token_info), std::move(callback));
}

void GCMClientImpl::DeleteToken(scoped_refptr<InstanceIDTokenInfo> token_info,
                                DeleteTokenCallback callback) {
  // TODO(crbug.com/589587357): Upgrade to `CHECK_EQ` once
  // `GCMDriverDesktop::OnStoreReset()` re-gates `delayed_task_controller_`.
  DCHECK_EQ(state_, READY);
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  CHECK(token_info);
  CHECK(callback);
  CHECK(registration_manager_);

  auto instance_id_iter =
      registration_manager_->instance_id_data().find(token_info->app_id);
  // Tokens must not be deleted when an InstanceID has not been created yet.
  CHECK(instance_id_iter != registration_manager_->instance_id_data().end());

  // Remove the token(s) from the cache and the store.
  // TODO(crbug.com/589661362): Remove it only when the request is successful.
  if (token_info->authorized_entity == "*" && token_info->scope == "*") {
    // If no token is found for the Instance ID, don't need to unregister since
    // the Instance ID is not sent to the server yet.
    if (!RemoveAllCachedInstanceIDTokens(token_info->app_id)) {
      std::move(callback).Run(SUCCESS);
      return;
    }
  } else if (!RemoveCachedRegistration(token_info)) {
    std::move(callback).Run(INVALID_PARAMETER);
    return;
  }

  auto request_handler = std::make_unique<InstanceIDDeleteTokenRequestHandler>(
      instance_id_iter->second.first, token_info->authorized_entity,
      token_info->scope,
      GCMRegistrationManagerImpl::ConstructGCMVersion(
          chrome_build_info_.version));
  std::string source_to_record =
      token_info->authorized_entity + "/" + token_info->scope;

  bool use_subtype = GCMRegistrationManagerImpl::InstanceIDUsesSubtypeForAppId(
      token_info->app_id);
  std::string category = use_subtype
                             ? chrome_build_info_.product_category_for_subtypes
                             : token_info->app_id;
  std::string subtype = use_subtype ? token_info->app_id : std::string();

  StartUnregistrationRequest(std::move(token_info), category, subtype,
                             std::move(request_handler), source_to_record,
                             std::move(callback));
}

void GCMClientImpl::WaitForInitialConnectionReady(base::OnceClosure callback) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug.com/589587357): Upgrade to `CHECK_EQ(state_, READY)` once
  // `GCMDriverDesktop::OnStoreReset()` re-gates `delayed_task_controller_`.
  if (state_ != READY) {
    return;
  }
  CHECK(mcs_client_);
  mcs_client_->WaitForInitialConnectionReady(std::move(callback));
}

void GCMClientImpl::StartCheckin() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  // Make sure no checkin is in progress.
  if (checkin_request_) {
    return;
  }

  checkin_proto::ChromeBuildProto chrome_build_proto;
  fcm::ToCheckinProtoVersion(chrome_build_info_, &chrome_build_proto);

  CheckinRequest::RequestInfo request_info(
      device_checkin_info_.android_id(), device_checkin_info_.secret(),
      gservices_settings_.digest(), chrome_build_proto);
  checkin_request_ = std::make_unique<CheckinRequest>(
      gservices_settings_.GetCheckinURL(), request_info,
      fcm::GetBackoffPolicy(),
      base::BindOnce(&GCMClientImpl::OnCheckinCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      url_loader_factory_, io_task_runner_, &recorder_);
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
  fcm::CheckinInfo checkin_info;
  checkin_info.set_android_id(checkin_response.android_id());
  checkin_info.set_secret(checkin_response.security_token());

  if (state_ == INITIAL_DEVICE_CHECKIN) {
    OnFirstTimeDeviceCheckinCompleted(checkin_info);
  } else {
    // checkin_info is not expected to change after a periodic checkin as it
    // would invalidate the registration IDs.
    DCHECK_EQ(READY, state_);
    DCHECK_EQ(device_checkin_info_.android_id(), checkin_info.android_id());
    DCHECK_EQ(device_checkin_info_.secret(), checkin_info.secret());
  }

  if (device_checkin_info_.IsValid()) {
    // First update G-services settings, as something might have changed.
    if (gservices_settings_.UpdateFromCheckinResponse(checkin_response)) {
      CHECK(registration_manager_);
      registration_manager_->SetGServicesSettings(gservices_settings_);
      gcm_store_->SetGServicesSettings(
          gservices_settings_.settings_map(), gservices_settings_.digest(),
          base::BindOnce(&GCMClientImpl::SetGServicesSettingsCallback,
                         weak_ptr_factory_.GetWeakPtr()));
    }

    last_checkin_time_ = clock_->Now();
    gcm_store_->SetLastCheckinInfo(
        last_checkin_time_,
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
  if (checkin_request_.get()) {
    return;
  }

  // There should be only one periodic checkin pending at a time. Removing
  // pending periodic checkin to schedule a new one.
  periodic_checkin_ptr_factory_.InvalidateWeakPtrs();

  base::TimeDelta time_to_next_checkin = GetTimeToNextCheckin();
  if (time_to_next_checkin.is_negative()) {
    time_to_next_checkin = base::TimeDelta();
  }

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
  // the only opportunity to notify observers. For example a partial reset
  // that deletes the "CURRENT" file will cause GCMStoreImpl to consider the DB
  // to no longer exist, in which case the next load will simply create a new
  // store rather than resetting it.
  observers_.Notify(&Observer::OnStoreReset);

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
  destroying_gcm_store_ptr_factory_.InvalidateWeakPtrs();
  device_checkin_info_.Reset();
  connection_factory_.reset();
  mcs_client_.reset();
  checkin_request_.reset();
  // Delete all of the pending registration and unregistration requests.
  if (registration_manager_) {
    registration_manager_->SetDeviceCheckinInfo(device_checkin_info_);
    registration_manager_->CancelPendingRequests();
  }
  pending_unregistration_requests_.clear();
}

void GCMClientImpl::Register(
    scoped_refptr<GCMRegistrationInfo> gcm_registration_info) {
  DCHECK_EQ(state_, READY);
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  CHECK(gcm_registration_info);
  CHECK(registration_manager_);

  // Registrations should never pass as an app_id the special category used
  // internally when registering with a subtype. See security note in
  // GCMClientImpl::HandleIncomingMessage.
  CHECK_NE(gcm_registration_info->app_id,
           chrome_build_info_.product_category_for_subtypes);

  if (std::optional<std::string> cached_registration_id =
          registration_manager_->GetCachedRegistrationId(
              gcm_registration_info)) {
    observers_.Notify(&Observer::OnRegisterFinished, gcm_registration_info,
                      *cached_registration_id, SUCCESS);
    return;
  }

  std::string senders;
  for (const auto& kv : gcm_registration_info->sender_ids) {
    if (!senders.empty()) {
      senders.append(",");
    }
    senders.append(kv);
  }
  auto request_handler =
      std::make_unique<GCMRegistrationRequestHandler>(senders);

  registration_manager_->StartRegistrationRequest(
      gcm_registration_info, /*category=*/gcm_registration_info->app_id,
      /*subtype=*/std::string(), std::move(request_handler),
      /*source_to_record=*/senders,
      base::BindOnce(&GCMClientImpl::OnRegisterFinished,
                     weak_ptr_factory_.GetWeakPtr(), gcm_registration_info));
}

void GCMClientImpl::OnRegisterFinished(
    scoped_refptr<GCMRegistrationInfo> gcm_registration_info,
    const std::string& registration_id,
    Result result) {
  observers_.Notify(&Observer::OnRegisterFinished,
                    std::move(gcm_registration_info), registration_id, result);
}

bool GCMClientImpl::ValidateRegistration(
    scoped_refptr<RegistrationInfo> registration_info,
    const std::string& registration_id) {
  DCHECK_EQ(state_, READY);
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  CHECK(registration_manager_);
  return registration_manager_->ValidateRegistration(
      std::move(registration_info), registration_id);
}

void GCMClientImpl::Unregister(
    scoped_refptr<GCMRegistrationInfo> gcm_registration_info) {
  DCHECK_EQ(state_, READY);
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  CHECK(gcm_registration_info);
  CHECK(registration_manager_);

  // Remove the registration from the cache and the store.
  // TODO(crbug.com/589661362): Remove it only when the request is successful.
  if (!RemoveCachedRegistration(gcm_registration_info)) {
    NotifyUnregisterFinished(std::move(gcm_registration_info),
                             INVALID_PARAMETER);
    return;
  }

  auto request_handler = std::make_unique<GCMUnregistrationRequestHandler>(
      gcm_registration_info->app_id);

  StartUnregistrationRequest(
      gcm_registration_info, /*category=*/gcm_registration_info->app_id,
      /*subtype=*/std::string(), std::move(request_handler),
      /*source_to_record=*/std::string(),
      base::BindOnce(&GCMClientImpl::NotifyUnregisterFinished,
                     weak_ptr_factory_.GetWeakPtr(), gcm_registration_info));
}

bool GCMClientImpl::RemoveCachedRegistration(
    const scoped_refptr<RegistrationInfo>& registration_info) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  CHECK(registration_manager_);

  RegistrationInfoMap& registrations =
      registration_manager_->mutable_registrations();
  auto iter = registrations.find(registration_info);
  if (iter == registrations.end()) {
    return false;
  }
  registrations.erase(iter);

  gcm_store_->RemoveRegistration(
      registration_info->GetSerializedKey(),
      base::BindOnce(&GCMClientImpl::UpdateRegistrationCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  return true;
}

bool GCMClientImpl::RemoveAllCachedInstanceIDTokens(const std::string& app_id) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  CHECK(registration_manager_);

  size_t removed_count = std::erase_if(
      registration_manager_->mutable_registrations(),
      [this, &app_id](const auto& entry) {
        const InstanceIDTokenInfo* cached_token_info =
            InstanceIDTokenInfo::FromRegistrationInfo(entry.first.get());
        if (!cached_token_info || cached_token_info->app_id != app_id) {
          return false;
        }
        gcm_store_->RemoveRegistration(
            cached_token_info->GetSerializedKey(),
            base::BindOnce(&GCMClientImpl::UpdateRegistrationCallback,
                           weak_ptr_factory_.GetWeakPtr()));
        return true;
      });
  return removed_count > 0;
}

void GCMClientImpl::StartUnregistrationRequest(
    scoped_refptr<RegistrationInfo> registration_info,
    const std::string& category,
    const std::string& subtype,
    std::unique_ptr<UnregistrationRequest::CustomRequestHandler>
        request_handler,
    const std::string& source_to_record,
    base::OnceCallback<void(Result result)> callback) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  UnregistrationRequest::RequestInfo request_info(
      device_checkin_info_.android_id(), device_checkin_info_.secret(),
      category, subtype);

  auto unregistration_request = std::make_unique<UnregistrationRequest>(
      gservices_settings_.GetRegistrationURL(), request_info,
      std::move(request_handler), fcm::GetBackoffPolicy(),
      base::BindOnce(&GCMClientImpl::OnUnregistrationRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr(), registration_info,
                     std::move(callback)),
      kMaxUnregistrationRetries, url_loader_factory_, io_task_runner_,
      &recorder_, source_to_record);
  unregistration_request->Start();
  pending_unregistration_requests_.emplace(std::move(registration_info),
                                           std::move(unregistration_request));
}

void GCMClientImpl::OnUnregistrationRequestCompleted(
    scoped_refptr<RegistrationInfo> registration_info,
    base::OnceCallback<void(Result result)> callback,
    UnregistrationRequest::Status status) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
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
  std::move(callback).Run(result);

  pending_unregistration_requests_.erase(registration_info);
}

void GCMClientImpl::NotifyUnregisterFinished(
    scoped_refptr<GCMRegistrationInfo> gcm_registration_info,
    Result result) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  observers_.Notify(&Observer::OnUnregisterFinished,
                    std::move(gcm_registration_info), result);
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
  NOTREACHED();
}

void GCMClientImpl::RecordDecryptionFailure(const std::string& app_id,
                                            fcm::FcmDecryptionResult result) {
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
  if (connection_factory_) {
    stats.connection_state = connection_factory_->GetConnectionStateString();
  }
  if (mcs_client_) {
    stats.send_queue_size = mcs_client_->GetSendQueueSize();
    stats.resend_queue_size = mcs_client_->GetResendQueueSize();
  }
  if (device_checkin_info_.android_id() > 0) {
    stats.android_id = device_checkin_info_.android_id();
  }
  if (device_checkin_info_.secret() > 0) {
    stats.android_secret = device_checkin_info_.secret();
  }

  recorder_.CollectActivities(&stats.recorded_activities);

  CHECK(registration_manager_);
  stats.registered_app_ids = registration_manager_->GetRegisteredAppIds();
  return stats;
}

void GCMClientImpl::OnActivityRecorded() {
  observers_.Notify(&Observer::OnActivityRecorded);
}

void GCMClientImpl::OnConnected(const GURL& current_server,
                                const net::IPEndPoint& ip_endpoint) {
  // TODO(gcm): expose current server in debug page.
  observers_.Notify(&Observer::OnActivityRecorded);
}

void GCMClientImpl::OnDisconnected() {
  observers_.Notify(&Observer::OnActivityRecorded);
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
      NOTREACHED() << "Message with unexpected tag received by GCMClient";
  }
}

void GCMClientImpl::OnMessageSentToMCS(const std::string& app_id,
                                       const std::string& message_id,
                                       MCSClient::MessageSendStatus status) {
  if (status == MCSClient::SENT) {
    // Send acknowledgments are intentionally ignored.
    return;
  }

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
    observers_.Notify(&Observer::OnMessageSendError, app_id,
                      send_error_details);
  } else {
    observers_.Notify(&Observer::OnSendFinished, app_id, message_id,
                      ToGCMClientResult(status));
  }
}

void GCMClientImpl::HandleIncomingMessage(const gcm::MCSMessage& message) {
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
      HandleIncomingDataMessage(app_id, data_message_stanza, message_data);
      break;
    case DELETED_MESSAGES:
      HandleIncomingDeletedMessages(app_id, data_message_stanza);
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
    const mcs_proto::DataMessageStanza& data_message_stanza,
    MessageData& message_data) {
  recorder_.RecordDataMessageReceived(app_id, data_message_stanza.from(),
                                      data_message_stanza.ByteSizeLong(),
                                      GCMStatsRecorder::DATA_MESSAGE);

  IncomingMessage incoming_message;
  incoming_message.sender_id = data_message_stanza.from();
  incoming_message.message_id = data_message_stanza.persistent_id();
  if (data_message_stanza.has_token()) {
    incoming_message.collapse_key = data_message_stanza.token();
  }
  incoming_message.data = message_data;
  incoming_message.raw_data = data_message_stanza.raw_data();

  observers_.Notify(&Observer::OnMessageReceived, app_id, incoming_message);
}

void GCMClientImpl::HandleIncomingDeletedMessages(
    const std::string& app_id,
    const mcs_proto::DataMessageStanza& data_message_stanza) {
  recorder_.RecordDataMessageReceived(app_id, data_message_stanza.from(),
                                      data_message_stanza.ByteSizeLong(),
                                      GCMStatsRecorder::DELETED_MESSAGES);
  observers_.Notify(&Observer::OnMessagesDeleted, app_id);
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
  observers_.Notify(&Observer::OnMessageSendError, app_id, send_error_details);
}

bool GCMClientImpl::HasStandaloneRegisteredApp() const {
  CHECK(registration_manager_);
  return registration_manager_->HasStandaloneRegisteredApp();
}

}  // namespace gcm
