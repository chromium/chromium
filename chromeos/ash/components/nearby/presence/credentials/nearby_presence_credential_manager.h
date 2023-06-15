// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_CREDENTIAL_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_CREDENTIAL_MANAGER_H_

#include "base/functional/callback_forward.h"

namespace ash::nearby::presence {

// The entry point for the credential web integration component. This class is
// responsible for communicating with the Nearby Presence library to fetch,
// save, and generate credentials. Exposed API's provide callers with
// information about whether the user's local device has been registered with
// the Nearby Presence server and the ability to trigger an immediate credential
// upload/download. In addition,this class is responsible for daily credential
// upload/downloads with the server.
class NearbyPresenceCredentialManager {
 public:
  NearbyPresenceCredentialManager() = default;
  virtual ~NearbyPresenceCredentialManager() = default;

  // Returns whether this is this device has been registered with the server
  // for NP before and completed the registration flow steps outlined in
  // `RegisterPresence`.
  virtual bool IsLocalDeviceRegistered() = 0;

  // Kicks off the first time initialization flow for registering presence
  // with the Nearby Presence server. Returns the success of registration.
  //
  // The flow for registration is as follows:
  //      1. Register this device with the server.
  //      2. Generate this device's credentials.
  //      3. Upload this device's credentials.
  //      4. Download other devices' credentials.
  //      5. Save other devices' credentials.
  //
  // Callers are expected to check |IsPresenceInitialized| and only call this
  // function when it is false.
  virtual void RegisterPresence(
      base::OnceCallback<void(bool)> on_registered_callback) = 0;

  // Schedules an immediate task to upload/download credentials to/from the
  // server.
  virtual void UpdateCredentials() = 0;
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_CREDENTIAL_MANAGER_H_
