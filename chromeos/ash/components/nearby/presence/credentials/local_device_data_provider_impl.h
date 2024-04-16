// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_LOCAL_DEVICE_DATA_PROVIDER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_LOCAL_DEVICE_DATA_PROVIDER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider.h"
#include "third_party/nearby/internal/proto/credential.pb.h"
#include "third_party/nearby/internal/proto/metadata.pb.h"

class PrefService;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash::nearby::presence {

class LocalDeviceDataProviderImpl : public LocalDeviceDataProvider {
 public:
  LocalDeviceDataProviderImpl(PrefService* pref_service,
                              signin::IdentityManager* identity_manager);
  ~LocalDeviceDataProviderImpl() override;

  LocalDeviceDataProviderImpl(LocalDeviceDataProviderImpl&) = delete;
  LocalDeviceDataProviderImpl& operator=(LocalDeviceDataProviderImpl&) = delete;

  // LocalDeviceDataProvider
  void UpdatePersistedSharedCredentials(
      const std::vector<::nearby::internal::SharedCredential>&
          new_shared_credentials) override;
  bool HaveSharedCredentialsChanged(
      const std::vector<::nearby::internal::SharedCredential>&
          new_shared_credentials) override;
  std::string GetDeviceId() override;
  ::nearby::internal::DeviceIdentityMetaData GetDeviceMetadata() override;
  std::string GetAccountName() override;
  void SaveUserRegistrationInfo(const std::string& display_name,
                                const std::string& image_url) override;
  bool IsRegistrationCompleteAndUserInfoSaved() override;
  void SetRegistrationComplete(bool complete) override;

 private:
  // Creates a device name of the form "<given name>'s <device type>."
  // For example, "Josh's Chromebook." If a given name cannot be found, returns
  // just the device type.
  std::string GetDeviceName() const;

  std::optional<std::string> FetchAndDecodeDeviceId();
  void EncodeAndPersistDeviceId(std::string raw_device_id_bytes);

  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_LOCAL_DEVICE_DATA_PROVIDER_IMPL_H_
