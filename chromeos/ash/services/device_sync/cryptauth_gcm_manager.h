// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_GCM_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_GCM_MANAGER_H_

#include <optional>
#include <string>

#include "chromeos/ash/services/device_sync/cryptauth_feature_type.h"

class PrefRegistrySimple;

namespace ash {

namespace device_sync {

// Interface for the manager controlling GCM registrations and handling GCM push
// messages for CryptAuth. CryptAuth sends GCM messages to request the local
// device to re-enroll to get the freshest device state, and to notify the
// local device to resync the remote device list when this list changes.
class CryptAuthGCMManager {
 public:
  class Observer {
   public:
    virtual ~Observer();

    // Called when a gcm registration attempt finishes with the |success| of the
    // attempt.
    virtual void OnGCMRegistrationResult(bool success);

    // Called when a GCM message is received to re-enroll the device with
    // CryptAuth.
    // |session_id|: Only included in messages sent by CryptAuth v2 DeviceSync
    //               and null otherwise. Value should be included in the
    //               session_id field of ClientMetadata.
    // |feature_type|: Only included in messages resulting from
    //                 BatchNotifyGroupDevices requests and null otherwise.
    virtual void OnReenrollMessage(
        const std::optional<std::string>& session_id,
        const std::optional<CryptAuthFeatureType>& feature_type);

    // Called when a GCM message is received to sync down new devices from
    // CryptAuth.
    // |session_id|: Only included in messages sent by CryptAuth v2 DeviceSync
    //               and null otherwise. Value should be included in the
    //               session_id field of ClientMetadata.
    // |feature_type|: Only included in messages resulting from
    //                 BatchNotifyGroupDevices requests and null otherwise.
    virtual void OnResyncMessage(
        const std::optional<std::string>& session_id,
        const std::optional<CryptAuthFeatureType>& feature_type);
  };

  virtual ~CryptAuthGCMManager() {}

  // Registers the prefs used by the manager to the given |pref_service|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Check if registration ID is deprecated.
  static bool IsRegistrationIdDeprecated(const std::string& registration_id);

  // Starts listening to incoming GCM messages. If GCM registration is completed
  // after this function is called, then messages will also be handled properly.
  virtual void StartListening() = 0;

  // Returns true after StartListening() is called.
  virtual bool IsListening() = 0;

  // Begins registration with GCM. The Observer::OnGCMRegistrationResult()
  // observer function will be called when registration completes.
  virtual void RegisterWithGCM() = 0;

  // Returns the GCM registration id received from the last successful
  // registration. If registration has not been performed, then an empty string
  // will be returned.
  virtual std::string GetRegistrationId() = 0;

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer.
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_GCM_MANAGER_H_
