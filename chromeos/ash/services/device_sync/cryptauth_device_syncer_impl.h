// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_SYNCER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_SYNCER_IMPL_H_

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_registry.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_syncer.h"
#include "chromeos/ash/services/device_sync/cryptauth_ecies_encryptor.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_status_getter.h"
#include "chromeos/ash/services/device_sync/cryptauth_group_private_key_sharer.h"
#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/cryptauth_metadata_syncer.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_better_together_device_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_directive.pb.h"

class PrefService;

namespace cryptauthv2 {
class ClientAppMetadata;
class ClientMetadata;
}  // namespace cryptauthv2

namespace ash {

namespace device_sync {

class AttestationCertificatesSyncer;
class CryptAuthClient;
class CryptAuthClientFactory;
class CryptAuthKeyRegistry;
class SyncedBluetoothAddressTracker;

// An implementation of CryptAuthDeviceSyncer, using instances of
// CryptAuthClient to make the API calls to CryptAuth. This implementation
// handles timeouts internally, so the callback passed to
// CryptAuthDeviceSyncer::Sync() is always guaranteed to be invoked.
//
// When the DeviceSync flow finishes, the device registry is updated with all
// devices that have a valid device ID, device name, device public key, and
// feature states. If device metadata cannot be decrypted due to an error or
// because the group private key was not returned by CryptAuth, the device is
// still added to the registry without the new decrypted metadata. Any existing
// decrypted metadata from the device registry will remain there until the new
// metadata can be decrypted.
class CryptAuthDeviceSyncerImpl : public CryptAuthDeviceSyncer {
 public:
  class Factory {
   public:
    static std::unique_ptr<CryptAuthDeviceSyncer> Create(
        CryptAuthDeviceRegistry* device_registry,
        CryptAuthKeyRegistry* key_registry,
        CryptAuthClientFactory* client_factory,
        SyncedBluetoothAddressTracker* synced_bluetooth_address_tracker,
        AttestationCertificatesSyncer* attestation_certificates_syncer,
        PrefService* pref_service,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthDeviceSyncer> CreateInstance(
        CryptAuthDeviceRegistry* device_registry,
        CryptAuthKeyRegistry* key_registry,
        CryptAuthClientFactory* client_factory,
        SyncedBluetoothAddressTracker* synced_bluetooth_address_tracker,
        AttestationCertificatesSyncer* attestation_certificates_syncer,
        PrefService* pref_service,
        std::unique_ptr<base::OneShotTimer> timer) = 0;

   private:
    static Factory* test_factory_;
  };

  CryptAuthDeviceSyncerImpl(const CryptAuthDeviceSyncerImpl&) = delete;
  CryptAuthDeviceSyncerImpl& operator=(const CryptAuthDeviceSyncerImpl&) =
      delete;

  ~CryptAuthDeviceSyncerImpl() override;

 private:
  enum class State {
    kNotStarted,
    kWaitingForBluetoothAddress,
    kWaitingForAttestationCertificates,
    kWaitingForMetadataSync,
    kWaitingForFeatureStatuses,
    kWaitingForEncryptedGroupPrivateKeyProcessing,
    kWaitingForEncryptedDeviceMetadataProcessing,
    kWaitingForGroupPrivateKeySharing,
    kFinished
  };

  friend std::ostream& operator<<(std::ostream& stream, const State& state);

  static std::optional<base::TimeDelta> GetTimeoutForState(State state);
  static std::optional<CryptAuthDeviceSyncResult::ResultCode>
  ResultCodeErrorFromTimeoutDuringState(State state);

  // |device_registry|: At the end of a DeviceSync flow, the devices in the
  //     registry are replaced with the devices received from CryptAuth.
  // |key_registry|: The syncer will read and possibly write the group key pair,
  //     and it will read the user key pair and the key used for decrypting the
  //     group private key.
  // |client_factory|: Creates CryptAuthClient instances for making API calls.
  // |synced_bluetooth_address_tracker|: Used to fetch Bluetooth address and
  //     track address used for successful syncs.
  // |timer|: Handles timeouts for asynchronous operations.
  CryptAuthDeviceSyncerImpl(
      CryptAuthDeviceRegistry* device_registry,
      CryptAuthKeyRegistry* key_registry,
      CryptAuthClientFactory* client_factory,
      SyncedBluetoothAddressTracker* synced_bluetooth_address_tracker,
      AttestationCertificatesSyncer* attestation_certificates_syncer,
      PrefService* pref_service,
      std::unique_ptr<base::OneShotTimer> timer);

  // CryptAuthDeviceSyncer:
  void OnAttemptStarted(
      const cryptauthv2::ClientMetadata& client_metadata,
      const cryptauthv2::ClientAppMetadata& client_app_metadata) override;

  void SetState(State state);
  void OnTimeout();

  // Controls the logical flow of the class.
  void AttemptNextStep();

  void GetBluetoothAddress();
  void OnBluetoothAddress(const std::string& bluetooth_address);

  void GetAttestationCertificates();
  void OnAttestationCertificates(const std::vector<std::string>& cert_chain,
                                 bool valid);

  void SyncMetadata();
  void OnSyncMetadataFinished(
      const CryptAuthMetadataSyncer::IdToDeviceMetadataPacketMap&
          id_to_device_metadata_packet_map,
      std::unique_ptr<CryptAuthKey> new_group_key,
      const std::optional<cryptauthv2::EncryptedGroupPrivateKey>&
          encrypted_group_private_key,
      const std::optional<cryptauthv2::ClientDirective>& new_client_directive,
      CryptAuthDeviceSyncResult::ResultCode device_sync_result_code);

  void SetGroupKey(const CryptAuthKey& new_group_key);

  void GetFeatureStatuses();
  void OnGetFeatureStatusesFinished(
      const CryptAuthFeatureStatusGetter::IdToDeviceSoftwareFeatureInfoMap&
          id_to_device_software_feature_info_map,
      CryptAuthDeviceSyncResult::ResultCode device_sync_result_code);

  // Builds a new device registry map with all device information except
  // decrypted BetterTogetherDeviceMetadata for remote devices.
  void BuildNewDeviceRegistry(
      const CryptAuthFeatureStatusGetter::IdToDeviceSoftwareFeatureInfoMap&
          id_to_device_software_feature_info_map);

  // If an encrypted group private key was sent by CryptAuth, decrypt it. Even
  // if we already have the unencrypted group private key in the key registry,
  // we verify that they agree.
  void ProcessEncryptedGroupPrivateKey();
  void OnGroupPrivateKeyDecrypted(
      const std::optional<std::string>& group_private_key_from_cryptauth);

  void ProcessEncryptedDeviceMetadata();
  void OnDeviceMetadataDecrypted(const CryptAuthEciesEncryptor::IdToOutputMap&
                                     id_to_decrypted_metadata_map);

  // Adds decrypted BetterTogetherDeviceMetadata to the new device registry
  // constructed in BuildNewDeviceRegistry().
  void AddDecryptedMetadataToNewDeviceRegistry(
      const CryptAuthEciesEncryptor::IdToOutputMap&
          id_to_decrypted_metadata_map);

  void ShareGroupPrivateKey();
  void OnShareGroupPrivateKeyFinished(
      CryptAuthDeviceSyncResult::ResultCode device_sync_result_code);

  // Replaces the current device registry if devices were able to be extracted
  // from the DeviceSync attempt. Finishes the DeviceSync attempt, sending back
  // the relevant CryptAuthDeviceSyncResult.
  void FinishAttempt(CryptAuthDeviceSyncResult::ResultCode result_code);

  bool did_non_fatal_error_occur_ = false;
  // Field reflects the certificate validity state only after local_better_together_device_metadata_
  // has been updated and before the sync attempt has completed. It is otherwise always false.
  bool are_attestation_certs_valid_ = false;

  // Set in OnAttemptStarted() and not modified during the rest of the flow.
  cryptauthv2::RequestContext request_context_;
  cryptauthv2::BetterTogetherDeviceMetadata
      local_better_together_device_metadata_;

  // Output from CryptAuthMetadataSyncer.
  CryptAuthMetadataSyncer::IdToDeviceMetadataPacketMap
      id_to_device_metadata_packet_map_;
  std::optional<cryptauthv2::EncryptedGroupPrivateKey>
      encrypted_group_private_key_;
  std::optional<cryptauthv2::ClientDirective> new_client_directive_;

  // Populated after a successful BatchGetFeatureStatuses call. Device metadata
  // is added if device metadata decryption is successful. Replaces the contents
  // of the device registry if non-null when the DeviceSync attempt ends,
  // successfully or not.
  std::optional<CryptAuthDeviceRegistry::InstanceIdToDeviceMap>
      new_device_registry_map_;

  // The time of the last state change. Used for execution time metrics.
  base::TimeTicks last_state_change_timestamp_;

  std::unique_ptr<CryptAuthMetadataSyncer> metadata_syncer_;
  std::unique_ptr<CryptAuthFeatureStatusGetter> feature_status_getter_;
  std::unique_ptr<CryptAuthEciesEncryptor> encryptor_;
  std::unique_ptr<CryptAuthGroupPrivateKeySharer> group_private_key_sharer_;

  State state_ = State::kNotStarted;
  raw_ptr<CryptAuthDeviceRegistry> device_registry_ = nullptr;
  raw_ptr<CryptAuthKeyRegistry> key_registry_ = nullptr;
  raw_ptr<CryptAuthClientFactory> client_factory_ = nullptr;
  raw_ptr<SyncedBluetoothAddressTracker> synced_bluetooth_address_tracker_ =
      nullptr;
  raw_ptr<AttestationCertificatesSyncer> attestation_certificates_syncer_ =
      nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;
  std::unique_ptr<base::OneShotTimer> timer_;

  base::WeakPtrFactory<CryptAuthDeviceSyncerImpl> weak_ptr_factory_{this};
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_SYNCER_IMPL_H_
