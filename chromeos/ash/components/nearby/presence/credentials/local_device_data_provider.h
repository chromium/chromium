// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_LOCAL_DEVICE_DATA_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_LOCAL_DEVICE_DATA_PROVIDER_H_

#include <memory>
#include <string>

namespace nearby::internal {
class SharedCredential;
class DeviceIdentityMetaData;
}  // namespace nearby::internal

namespace ash::nearby::presence {

// Provides data for the local device: manages local device's shared
// credentials, account information, and constructs the Metadata object.
class LocalDeviceDataProvider {
 public:
  LocalDeviceDataProvider() = default;
  virtual ~LocalDeviceDataProvider() = default;

  // Updates the persisted shared credential ids saved to prefs.
  virtual void UpdatePersistedSharedCredentials(
      const std::vector<::nearby::internal::SharedCredential>&
          new_shared_credentials) = 0;

  // Returns true if the shared credentials for the local device have
  // changed.
  virtual bool HaveSharedCredentialsChanged(
      const std::vector<::nearby::internal::SharedCredential>&
          new_shared_credentials) = 0;

  // Returns the unique device identifier if it exists. If not, generates a
  // unique device identifier, persists to prefs, and returns it.
  virtual std::string GetDeviceId() = 0;

  // Constructs and returns metadata for the local device.
  virtual ::nearby::internal::DeviceIdentityMetaData GetDeviceMetadata() = 0;

  // Returns the cacancolized account name for the user.
  virtual std::string GetAccountName() = 0;

  // Persists first time registration information returned from the server
  // to Prefs to be accessed during Metadata construction
  virtual void SaveUserRegistrationInfo(const std::string& display_name,
                                        const std::string& image_url) = 0;

  // Checks if the first time registration information returned from the
  // server is persisted to prefs, and the full registration flow has been
  // completed successfully.
  virtual bool IsRegistrationCompleteAndUserInfoSaved() = 0;

  // Persists a boolean indicating that the full registration flow has been
  // completed.
  virtual void SetRegistrationComplete(bool complete) = 0;
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_LOCAL_DEVICE_DATA_PROVIDER_H_
