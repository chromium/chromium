// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_GCM_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_GCM_MANAGER_H_

#include <optional>
#include <string>

#include "base/observer_list.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/ash/services/device_sync/cryptauth_gcm_manager.h"

namespace ash {

namespace device_sync {

// CryptAuthGCMManager implementation for testing purposes.
class FakeCryptAuthGCMManager : public CryptAuthGCMManager {
 public:
  // Creates the instance:
  // |registration_id|: The GCM registration id from a previous successful
  //     enrollment. Pass in an empty |registration_id| to simulate never having
  //     registered successfully.
  explicit FakeCryptAuthGCMManager(const std::string& registration_id);

  FakeCryptAuthGCMManager(const FakeCryptAuthGCMManager&) = delete;
  FakeCryptAuthGCMManager& operator=(const FakeCryptAuthGCMManager&) = delete;

  ~FakeCryptAuthGCMManager() override;

  bool registration_in_progress() { return registration_in_progress_; }

  void set_registration_id(const std::string& registration_id) {
    registration_id_ = registration_id;
  }

  // Simulates completing a GCM registration with the resulting
  // |registration_id|. Passing an empty |registration_id| will simulate a
  // registration failure.
  // A registration attempt must currently be in progress.
  void CompleteRegistration(const std::string& registration_id);

  // Simulates receiving a re-enroll push message from GCM.
  void PushReenrollMessage(
      const std::optional<std::string>& session_id,
      const std::optional<CryptAuthFeatureType>& feature_type);

  // Simulates receiving a re-sync push message from GCM.
  void PushResyncMessage(
      const std::optional<std::string>& session_id,
      const std::optional<CryptAuthFeatureType>& feature_type);

  // CryptAuthGCMManager:
  void StartListening() override;
  bool IsListening() override;
  void RegisterWithGCM() override;
  std::string GetRegistrationId() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  bool is_listening_ = false;
  bool registration_in_progress_ = false;

  // The registration id obtained from the last successful registration.
  std::string registration_id_;

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_GCM_MANAGER_H_
