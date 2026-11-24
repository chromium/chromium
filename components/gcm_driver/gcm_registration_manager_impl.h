// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_REGISTRATION_MANAGER_IMPL_H_
#define COMPONENTS_GCM_DRIVER_GCM_REGISTRATION_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/fcm/engine/checkin_info.h"
#include "components/fcm/engine/chrome_build_info.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_registration_manager.h"
#include "google_apis/gcm/engine/gcm_store.h"
#include "google_apis/gcm/engine/gservices_settings.h"
#include "google_apis/gcm/engine/registration_request.h"

namespace base {
class Clock;
class SequencedTaskRunner;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace gcm {

class GCMStatsRecorder;

// `GCMRegistrationManager` implementation that caches and persists registration
// and Instance ID data in `GCMStore`.
class GCMRegistrationManagerImpl : public GCMRegistrationManager {
 public:
  using InstanceIDDataMap =
      std::map<std::string, std::pair<std::string, std::string>>;
  using RegisterCallback =
      base::OnceCallback<void(const std::string& registration_id,
                              GCMClient::Result result)>;

  // `gcm_store`, `clock`, and `recorder` must outlive this instance.
  GCMRegistrationManagerImpl(
      GCMStore& gcm_store,
      base::Clock& clock,
      fcm::ChromeBuildInfo chrome_build_info,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      GCMStatsRecorder& recorder);

  GCMRegistrationManagerImpl(const GCMRegistrationManagerImpl&) = delete;
  GCMRegistrationManagerImpl& operator=(const GCMRegistrationManagerImpl&) =
      delete;

  ~GCMRegistrationManagerImpl() override;

  // Updates the device check-in credentials used for registration requests.
  void SetDeviceCheckinInfo(const fcm::CheckinInfo& device_checkin_info);

  // Updates the G-services settings used to look up the registration URL.
  void SetGServicesSettings(const GServicesSettings& gservices_settings);

  // Populates cached registration and Instance ID data from `load_result`.
  void OnRegistrationDataLoaded(const GCMStore::LoadResult& load_result);

  // Returns true if at least one standalone app is registered for GCM.
  bool HasStandaloneRegisteredApp() const;

  // Cancels all pending registration requests.
  void CancelPendingRequests();

  // Returns the cached registration ID if a matching, non-stale registration
  // exists in `registrations_`, or `std::nullopt` otherwise.
  std::optional<std::string> GetCachedRegistrationId(
      scoped_refptr<RegistrationInfo> registration_info) const;

  // TODO(b/545115204): Make private once `Register()` is migrated from
  // `GCMClientImpl`.
  // Starts a registration request for `registration_info` and tracks it as
  // pending, invoking `callback` upon completion.
  void StartRegistrationRequest(
      scoped_refptr<RegistrationInfo> registration_info,
      const std::string& category,
      const std::string& subtype,
      std::unique_ptr<RegistrationRequest::CustomRequestHandler>
          request_handler,
      const std::string& source_to_record,
      RegisterCallback callback);

  // Tracks an in-flight `request` for `registration_info`.
  void AddPendingRegistrationRequest(
      scoped_refptr<RegistrationInfo> registration_info,
      std::unique_ptr<RegistrationRequest> request);

  // Completes a pending registration request, updating the cache and persistent
  // store on success and invoking `callback` upon completion.
  void OnRegisterCompleted(scoped_refptr<RegistrationInfo> registration_info,
                           RegisterCallback callback,
                           RegistrationRequest::Status status,
                           const std::string& registration_id);

  // TODO(b/545115204): Move to the anonymous namespace once `DeleteToken()` is
  // migrated from `GCMClientImpl`.
  static int ConstructGCMVersion(const std::string& chrome_version);
  static bool InstanceIDUsesSubtypeForAppId(const std::string& app_id);

  // Returns a `WeakPtr` invalidated by `CancelPendingRequests()`.
  base::WeakPtr<GCMRegistrationManagerImpl> GetPendingRequestWeakPtr() {
    return pending_requests_weak_ptr_factory_.GetWeakPtr();
  }

  // GCMRegistrationManager implementation:
  void GetToken(scoped_refptr<InstanceIDTokenInfo> token_info,
                GetTokenCallback callback) override;
  bool ValidateRegistration(scoped_refptr<RegistrationInfo> registration_info,
                            const std::string& registration_id) override;
  std::vector<std::string> GetRegisteredAppIds() const override;
  void AddInstanceIDData(const std::string& app_id,
                         const std::string& instance_id,
                         const std::string& extra_data) override;
  void RemoveInstanceIDData(const std::string& app_id) override;
  std::pair<std::string, std::string> GetInstanceIDData(
      const std::string& app_id) const override;

  // TODO(b/545115204): Remove temporary accessors once all registration and
  // Instance ID methods and tests are migrated from `GCMClientImpl`.
  const RegistrationInfoMap& registrations() const { return registrations_; }
  RegistrationInfoMap& mutable_registrations() { return registrations_; }
  const InstanceIDDataMap& instance_id_data() const {
    return instance_id_data_;
  }

 private:
  // Collection of pending registration requests. Keys are registration info
  // instances, while values are pending registration requests to obtain a
  // registration ID for requesting application, or for InstanceID tokens, the
  // scope and extra data as well.
  using PendingRegistrationRequests =
      std::map<scoped_refptr<RegistrationInfo>,
               std::unique_ptr<RegistrationRequest>,
               RegistrationInfoComparer>;

  // Removes deprecated data (such as obsolete registrations) from the cache and
  // persistent store.
  void RemoveDeprecatedData();

  // Callback for persisting registration info in `gcm_store_`.
  void UpdateRegistrationCallback(bool success);

  const raw_ref<GCMStore> gcm_store_;
  const raw_ref<base::Clock> clock_;
  const fcm::ChromeBuildInfo chrome_build_info_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  const raw_ref<GCMStatsRecorder> recorder_;

  fcm::CheckinInfo device_checkin_info_;
  GServicesSettings gservices_settings_;

  // Cached registration info.
  RegistrationInfoMap registrations_;

  // Cached Instance ID data, key is app ID and value is pair of Instance ID
  // and extra data.
  InstanceIDDataMap instance_id_data_;

  // Currently pending registration requests. GCMRegistrationManagerImpl owns
  // the RegistrationRequests.
  PendingRegistrationRequests pending_registration_requests_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Invalidated by `CancelPendingRequests()` when the client stops.
  base::WeakPtrFactory<GCMRegistrationManagerImpl>
      pending_requests_weak_ptr_factory_{this};
  base::WeakPtrFactory<GCMRegistrationManagerImpl> weak_ptr_factory_{this};
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_REGISTRATION_MANAGER_IMPL_H_
