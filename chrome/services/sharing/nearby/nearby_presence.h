// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_NEARBY_PRESENCE_H_
#define CHROME_SERVICES_SHARING_NEARBY_NEARBY_PRESENCE_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/connections/listeners.h"
#include "third_party/nearby/src/internal/interop/device_provider.h"
#include "third_party/nearby/src/presence/presence_client.h"
#include "third_party/nearby/src/presence/presence_service.h"

namespace ash::nearby::presence {

// Implementation of the NearbyPresence mojo interface.
// This class acts as a bridge to the NearbyPresence library which is pulled in
// as a third_party dependency. It handles the translation from mojo calls to
// native callbacks and types that the library expects. This class runs in a
// sandboxed process and is called from the browser process.
class NearbyPresence : public mojom::NearbyPresence {
 public:
  // Creates a new instance of the NearbyPresence library. This will allocate
  // and initialize a new instance and hold on to the passed mojo pipes.
  // |on_disconnect| is called when either mojo interface disconnects and should
  // destroy this instamce.
  NearbyPresence(mojo::PendingReceiver<mojom::NearbyPresence> nearby_presence,
                 base::OnceClosure on_disconnect);
  NearbyPresence(const NearbyPresence&) = delete;
  NearbyPresence& operator=(const NearbyPresence&) = delete;
  ~NearbyPresence() override;

  ::nearby::NearbyDeviceProvider* GetLocalDeviceProvider() {
    return presence_service_->GetLocalDeviceProvider();
  }

 protected:
  NearbyPresence(
      std::unique_ptr<::nearby::presence::PresenceService> presence_service,
      mojo::PendingReceiver<mojom::NearbyPresence> nearby_presence,
      base::OnceClosure on_disconnect);

 private:
  class ScanSessionImpl : public mojom::ScanSession {
   public:
    ScanSessionImpl();
    ~ScanSessionImpl() override;
    mojo::Receiver<mojom::ScanSession> receiver{this};
  };

  friend class NearbyPresenceTest;
  FRIEND_TEST_ALL_PREFIXES(NearbyPresenceTest, RunStartScan_StatusOk);
  FRIEND_TEST_ALL_PREFIXES(NearbyPresenceTest, RunStartScan_StatusNotOk);
  FRIEND_TEST_ALL_PREFIXES(NearbyPresenceTest,
                           RunStartScan_DeviceFoundCallback);
  FRIEND_TEST_ALL_PREFIXES(NearbyPresenceTest,
                           RunStartScan_DeviceChangedCallback);
  FRIEND_TEST_ALL_PREFIXES(NearbyPresenceTest, RunStartScan_DeviceLostCallback);
  FRIEND_TEST_ALL_PREFIXES(
      NearbyPresenceTest,
      UpdateLocalDeviceMetadataAndGenerateCredentials_Success);
  FRIEND_TEST_ALL_PREFIXES(
      NearbyPresenceTest,
      UpdateLocalDeviceMetadataAndGenerateCredentials_Fail);
  FRIEND_TEST_ALL_PREFIXES(NearbyPresenceTest, RunUpdateLocalDeviceMetadata);
  FRIEND_TEST_ALL_PREFIXES(NearbyPresenceTest,
                           UpdateRemoteSharedCredentials_Success);
  FRIEND_TEST_ALL_PREFIXES(NearbyPresenceTest,
                           UpdateRemoteSharedCredentials_Fail);
  FRIEND_TEST_ALL_PREFIXES(NearbyPresenceTest,
                           GetLocalSharedCredentials_Success);
  FRIEND_TEST_ALL_PREFIXES(NearbyPresenceTest,
                           GetLocalSharedCredentials_Failure);

  // mojom::NearbyPresence:
  void SetScanObserver(
      mojo::PendingRemote<mojom::ScanObserver> scan_observer) override;
  void StartScan(mojom::ScanRequestPtr scan_request,
                 StartScanCallback callback) override;
  void UpdateLocalDeviceMetadata(mojom::MetadataPtr metadata) override;
  void UpdateLocalDeviceMetadataAndGenerateCredentials(
      mojom::MetadataPtr metadata,
      UpdateLocalDeviceMetadataAndGenerateCredentialsCallback callback)
      override;
  void UpdateRemoteSharedCredentials(
      std::vector<mojom::SharedCredentialPtr> shared_credentials,
      const std::string& account_name,
      UpdateRemoteSharedCredentialsCallback callback) override;
  void GetLocalSharedCredentials(
      const std::string& account_name,
      GetLocalSharedCredentialsCallback callback) override;

  // This is used as the disconnect handler for ScanSession.
  void OnScanSessionDisconnect(uint64_t scan_session_id);

  uint64_t id_ = 0;
  mojo::SharedRemote<mojom::ScanObserver> scan_observer_remote_;

  std::unique_ptr<::nearby::presence::PresenceService> presence_service_;
  std::unique_ptr<::nearby::presence::PresenceClient> presence_client_;

  base::flat_map<uint64_t, std::unique_ptr<ScanSessionImpl>>
      session_id_to_scan_session_map_;
  base::flat_map<uint64_t, StartScanCallback>
      session_id_to_results_callback_map_;
  base::flat_map<uint64_t, mojo::PendingRemote<mojom::ScanSession>>
      session_id_to_scan_session_remote_map_;
  base::flat_map<uint64_t, uint64_t> id_to_session_id_map_;

  mojo::Receiver<mojom::NearbyPresence> nearby_presence_;

  base::WeakPtrFactory<NearbyPresence> weak_ptr_factory_{this};
};

}  // namespace ash::nearby::presence

#endif  // CHROME_SERVICES_SHARING_NEARBY_NEARBY_PRESENCE_H_
