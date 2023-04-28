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
  // for NP before.
  virtual bool IsLocalDeviceRegistered() = 0;

  // Kicks off the first time initialization flow for registering presence
  // with the Nearby Presence server. Returns the success of registration.
  //
  // The registration flow is as follows:
  // 1. Register with the NP server to make itself known as a device associated
  // with the user's GAIA.
  // 2. Generate local device’s credential pairs in NP library and upload to
  // the server.
  // 3. Download remote devices’ shared credentials and save to NP library.
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
