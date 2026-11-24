// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_registration_manager_impl.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/crx_file/id_util.h"
#include "components/fcm/base/fcm_backoff_policy.h"
#include "components/fcm/engine/checkin_info.h"
#include "google_apis/gcm/base/gcm_features.h"
#include "google_apis/gcm/engine/gcm_store.h"
#include "google_apis/gcm/engine/gservices_settings.h"
#include "google_apis/gcm/engine/instance_id_get_token_request_handler.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace gcm {

namespace {

// Maximum number of retry attempts for a failed registration or token request.
constexpr int kMaxRegistrationRetries = 5;

// Client-side cache freshness threshold after which a cached registration/token
// is treated as stale and re-validated with the server on the next
// registration/token request (registrations do not expire on the server after 7
// days).
constexpr base::TimeDelta kTokenInvalidationInterval = base::Days(7);

// Previously, GCMAccountMapper registered with this app ID. Any persisted
// registration for it in GCMStore is obsolete and removed on load.
// Deprecated in October 2026.
constexpr char kDeprecatedGCMAccountMapperAppId[] = "com.google.android.gms";

// Serializes `instance_id` and `extra_data` into "<instance_id>,<extra_data>".
std::string SerializeInstanceIDData(const std::string& instance_id,
                                    const std::string& extra_data) {
  CHECK(!instance_id.empty() && !extra_data.empty());
  CHECK(instance_id.find(',') == std::string::npos);
  return instance_id + "," + extra_data;
}

// Deserializes "<instance_id>,<extra_data>" into `{instance_id, extra_data}`,
// or returns `std::nullopt` if `serialized_data` is malformed or either field
// is empty.
std::optional<std::pair<std::string, std::string>> DeserializeInstanceIDData(
    std::string_view serialized_data) {
  std::optional<std::pair<std::string_view, std::string_view>> parts =
      base::SplitStringOnce(serialized_data, ',');
  if (!parts || parts->first.empty() || parts->second.empty()) {
    return std::nullopt;
  }
  return std::make_pair(std::string(parts->first), std::string(parts->second));
}

}  // namespace

GCMRegistrationManagerImpl::GCMRegistrationManagerImpl(
    GCMStore& gcm_store,
    base::Clock& clock,
    fcm::ChromeBuildInfo chrome_build_info,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    GCMStatsRecorder& recorder)
    : gcm_store_(gcm_store),
      clock_(clock),
      chrome_build_info_(std::move(chrome_build_info)),
      url_loader_factory_(std::move(url_loader_factory)),
      io_task_runner_(std::move(io_task_runner)),
      recorder_(recorder) {}

GCMRegistrationManagerImpl::~GCMRegistrationManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void GCMRegistrationManagerImpl::SetDeviceCheckinInfo(
    const fcm::CheckinInfo& device_checkin_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  device_checkin_info_ = device_checkin_info;
}

void GCMRegistrationManagerImpl::SetGServicesSettings(
    const GServicesSettings& gservices_settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gservices_settings_ = gservices_settings;
}

void GCMRegistrationManagerImpl::OnRegistrationDataLoaded(
    const GCMStore::LoadResult& load_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [key, value] : load_result.registrations) {
    std::string registration_id;
    scoped_refptr<RegistrationInfo> registration =
        RegistrationInfo::BuildFromString(key, value, &registration_id);
    // TODO(jianli): Add UMA to track the error case.
    if (registration) {
      registrations_.emplace(std::move(registration), registration_id);
    }
  }

  for (const auto& [key, value] : load_result.instance_id_data) {
    if (std::optional<std::pair<std::string, std::string>> data =
            DeserializeInstanceIDData(value)) {
      instance_id_data_[key] = *std::move(data);
    }
  }

  RemoveDeprecatedData();
}

bool GCMRegistrationManagerImpl::HasStandaloneRegisteredApp() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !registrations_.empty();
}

void GCMRegistrationManagerImpl::CancelPendingRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_registration_requests_.clear();
  pending_requests_weak_ptr_factory_.InvalidateWeakPtrs();
}

std::optional<std::string> GCMRegistrationManagerImpl::GetCachedRegistrationId(
    scoped_refptr<RegistrationInfo> registration_info) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // `RegistrationInfoComparer` matches `GCMRegistrationInfo` by `app_id` alone
  // and `InstanceIDTokenInfo` by `(app_id, authorized_entity, scope)`, so
  // `sender_ids` and `last_validated` must be read from the cached map key
  // (`registrations_iter->first`) rather than the lookup `registration_info`.
  RegistrationInfoMap::const_iterator registrations_iter =
      registrations_.find(registration_info);
  if (registrations_iter == registrations_.end()) {
    return std::nullopt;
  }

  // For GCM registration, also match the sender IDs since multiple
  // registrations are not supported.
  const GCMRegistrationInfo* gcm_registration_info =
      GCMRegistrationInfo::FromRegistrationInfo(registration_info.get());
  if (gcm_registration_info) {
    const GCMRegistrationInfo* cached_gcm_registration_info =
        GCMRegistrationInfo::FromRegistrationInfo(
            registrations_iter->first.get());
    CHECK(cached_gcm_registration_info);
    if (gcm_registration_info->sender_ids !=
        cached_gcm_registration_info->sender_ids) {
      return std::nullopt;
    }
  }

  // Do not reuse the cached registration if it has become stale.
  base::TimeDelta time_since_last_validation =
      clock_->Now() - registrations_iter->first->last_validated;
  if (time_since_last_validation >= kTokenInvalidationInterval) {
    return std::nullopt;
  }

  return registrations_iter->second;
}

void GCMRegistrationManagerImpl::StartRegistrationRequest(
    scoped_refptr<RegistrationInfo> registration_info,
    const std::string& category,
    const std::string& subtype,
    std::unique_ptr<RegistrationRequest::CustomRequestHandler> request_handler,
    const std::string& source_to_record,
    RegisterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RegistrationRequest::RequestInfo request_info(
      device_checkin_info_.android_id(), device_checkin_info_.secret(),
      category, subtype);

  auto registration_request = std::make_unique<RegistrationRequest>(
      gservices_settings_.GetRegistrationURL(), request_info,
      std::move(request_handler), fcm::GetBackoffPolicy(),
      base::BindOnce(&GCMRegistrationManagerImpl::OnRegisterCompleted,
                     pending_requests_weak_ptr_factory_.GetWeakPtr(),
                     registration_info, std::move(callback)),
      kMaxRegistrationRetries, url_loader_factory_, io_task_runner_,
      &recorder_.get(), source_to_record);
  registration_request->Start();
  AddPendingRegistrationRequest(std::move(registration_info),
                                std::move(registration_request));
}

void GCMRegistrationManagerImpl::AddPendingRegistrationRequest(
    scoped_refptr<RegistrationInfo> registration_info,
    std::unique_ptr<RegistrationRequest> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_registration_requests_.emplace(std::move(registration_info),
                                         std::move(request));
}

void GCMRegistrationManagerImpl::OnRegisterCompleted(
    scoped_refptr<RegistrationInfo> registration_info,
    RegisterCallback callback,
    RegistrationRequest::Status status,
    const std::string& registration_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GCMClient::Result result;
  if (pending_registration_requests_.erase(registration_info) == 0) {
    result = GCMClient::UNKNOWN_ERROR;
  } else if (status == RegistrationRequest::INVALID_SENDER) {
    result = GCMClient::INVALID_PARAMETER;
  } else if (registration_id.empty()) {
    // All other errors are currently treated as SERVER_ERROR (including
    // REACHED_MAX_RETRIES due to the device being offline!).
    result = GCMClient::SERVER_ERROR;
  } else {
    result = GCMClient::SUCCESS;
  }

  if (result == GCMClient::SUCCESS) {
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
        base::BindOnce(&GCMRegistrationManagerImpl::UpdateRegistrationCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  std::move(callback).Run(
      result == GCMClient::SUCCESS ? registration_id : std::string(), result);
}

// static
int GCMRegistrationManagerImpl::ConstructGCMVersion(
    const std::string& chrome_version) {
  // Major Chrome version is passed as GCM version.
  std::optional<std::pair<std::string_view, std::string_view>> parts =
      base::SplitStringOnce(chrome_version, '.');
  if (!parts) {
    NOTREACHED();
  }

  int gcm_version = 0;
  base::StringToInt(parts->first, &gcm_version);
  return gcm_version;
}

// static
bool GCMRegistrationManagerImpl::InstanceIDUsesSubtypeForAppId(
    const std::string& app_id) {
  // Always use subtypes with Instance ID, except for Chrome Apps/Extensions.
  return !crx_file::id_util::IdIsValid(app_id);
}

void GCMRegistrationManagerImpl::GetToken(
    scoped_refptr<InstanceIDTokenInfo> token_info,
    GetTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/589587357): Upgrade readiness `DCHECK`s to `CHECK`s once
  // `GCMDriverDesktop::OnStoreReset()` re-gates `delayed_task_controller_`.
  DCHECK(device_checkin_info_.IsValid());
  CHECK(token_info);
  CHECK(callback);

  // Registrations should never pass as an app_id the special category used
  // internally when registering with a subtype. See security note in
  // GCMClientImpl::HandleIncomingMessage.
  CHECK_NE(token_info->app_id,
           chrome_build_info_.product_category_for_subtypes);

  if (std::optional<std::string> cached_token =
          GetCachedRegistrationId(token_info)) {
    std::move(callback).Run(*cached_token, GCMClient::SUCCESS);
    return;
  }

  auto instance_id_iter = instance_id_data_.find(token_info->app_id);
  CHECK(instance_id_iter != instance_id_data_.end());

  auto request_handler = std::make_unique<InstanceIDGetTokenRequestHandler>(
      instance_id_iter->second.first, token_info->authorized_entity,
      token_info->scope, ConstructGCMVersion(chrome_build_info_.version),
      token_info->time_to_live);
  std::string source_to_record =
      token_info->authorized_entity + "/" + token_info->scope;

  bool use_subtype = InstanceIDUsesSubtypeForAppId(token_info->app_id);
  std::string category = use_subtype
                             ? chrome_build_info_.product_category_for_subtypes
                             : token_info->app_id;
  std::string subtype = use_subtype ? token_info->app_id : std::string();

  StartRegistrationRequest(std::move(token_info), category, subtype,
                           std::move(request_handler), source_to_record,
                           std::move(callback));
}

bool GCMRegistrationManagerImpl::ValidateRegistration(
    scoped_refptr<RegistrationInfo> registration_info,
    const std::string& registration_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Must have a cached registration.
  RegistrationInfoMap::const_iterator registrations_iter =
      registrations_.find(registration_info);
  if (registrations_iter == registrations_.end()) {
    return false;
  }

  // Cached registration ID must match.
  const std::string& cached_registration_id = registrations_iter->second;
  if (registration_id != cached_registration_id) {
    return false;
  }

  // For GCM registration, also match the sender IDs since multiple
  // registrations are not supported.
  const GCMRegistrationInfo* gcm_registration_info =
      GCMRegistrationInfo::FromRegistrationInfo(registration_info.get());
  if (gcm_registration_info) {
    const GCMRegistrationInfo* cached_gcm_registration_info =
        GCMRegistrationInfo::FromRegistrationInfo(
            registrations_iter->first.get());
    CHECK(cached_gcm_registration_info);
    if (gcm_registration_info->sender_ids !=
        cached_gcm_registration_info->sender_ids) {
      return false;
    }
  }

  return true;
}

std::vector<std::string> GCMRegistrationManagerImpl::GetRegisteredAppIds()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::string> app_ids;
  for (const auto& [registration_info, registration_id] : registrations_) {
    app_ids.push_back(registration_info->app_id);
  }
  return app_ids;
}

void GCMRegistrationManagerImpl::AddInstanceIDData(
    const std::string& app_id,
    const std::string& instance_id,
    const std::string& extra_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `InstanceIDImpl` re-sends the Instance ID before every token operation;
  // skip unchanged entries to avoid a synchronous LevelDB write on startup.
  auto iter = instance_id_data_.find(app_id);
  if (iter != instance_id_data_.end() && iter->second.first == instance_id &&
      iter->second.second == extra_data) {
    return;
  }
  instance_id_data_[app_id] = std::make_pair(instance_id, extra_data);
  // TODO(crbug.com/40109289): If this call fails, a registration may be leaked
  // (the entry stored in `instance_id_data_` would be used for a registration
  // but not persisted).
  gcm_store_->AddInstanceIDData(
      app_id, SerializeInstanceIDData(instance_id, extra_data),
      base::DoNothing());
}

void GCMRegistrationManagerImpl::RemoveInstanceIDData(
    const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  instance_id_data_.erase(app_id);
  // TODO(crbug.com/40691191): Implement proper error handling.
  gcm_store_->RemoveInstanceIDData(app_id, base::DoNothing());
}

std::pair<std::string, std::string>
GCMRegistrationManagerImpl::GetInstanceIDData(const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = instance_id_data_.find(app_id);
  if (iter == instance_id_data_.end()) {
    return {};
  }
  return iter->second;
}

void GCMRegistrationManagerImpl::RemoveDeprecatedData() {
  if (!base::FeatureList::IsEnabled(features::kGCMRemoveDeprecatedStoreData)) {
    return;
  }
  auto deprecated_info = base::MakeRefCounted<GCMRegistrationInfo>();
  deprecated_info->app_id = kDeprecatedGCMAccountMapperAppId;
  if (registrations_.erase(deprecated_info)) {
    gcm_store_->RemoveRegistration(kDeprecatedGCMAccountMapperAppId,
                                   base::DoNothing());
  }
}

void GCMRegistrationManagerImpl::UpdateRegistrationCallback(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40691191): Rebuild the store on write failure.
  DCHECK(success);
}

}  // namespace gcm
