// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_presence.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/services/sharing/nearby/nearby_shared_remotes.h"
#include "chromeos/ash/components/nearby/presence/conversions/nearby_presence_conversions.h"
#include "components/cross_device/logging/logging.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/nearby/internal/proto/credential.pb.h"
#include "third_party/nearby/src/presence/presence_client_impl.h"
#include "third_party/nearby/src/presence/presence_service_impl.h"

namespace {

constexpr char kChromeOSManagerAppId[] = "CHROMEOS";
constexpr int kCredentialLifeCycleDays = 5;
constexpr int kNumCredentials = 6;

void PostStartScanCallbackOnSequence(
    ash::nearby::presence::NearbyPresence::StartScanCallback callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<ash::nearby::presence::mojom::ScanSession> scan_session,
    mojo_base::mojom::AbslStatusCode status) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(scan_session), status));
}

}  // namespace

namespace ash::nearby::presence {

NearbyPresence::NearbyPresence(
    mojo::PendingReceiver<mojom::NearbyPresence> nearby_presence,
    base::OnceClosure on_disconnect)
    : NearbyPresence(
          std::make_unique<::nearby::presence::PresenceServiceImpl>(),
          std::move(nearby_presence),
          std::move(on_disconnect)) {}

NearbyPresence::NearbyPresence(
    std::unique_ptr<::nearby::presence::PresenceService> presence_service,
    mojo::PendingReceiver<mojom::NearbyPresence> nearby_presence,
    base::OnceClosure on_disconnect)
    : presence_service_(std::move(presence_service)),
      presence_client_(presence_service_->CreatePresenceClient()),
      nearby_presence_(this, std::move(nearby_presence)) {
  nearby_presence_.set_disconnect_handler(std::move(on_disconnect));
}

NearbyPresence::~NearbyPresence() = default;

void NearbyPresence::SetScanObserver(
    mojo::PendingRemote<mojom::ScanObserver> scan_observer) {
  scan_observer_remote_.Bind(std::move(scan_observer), /*task_runner=*/nullptr);
}

void NearbyPresence::StartScan(mojom::ScanRequestPtr scan_request,
                               StartScanCallback callback) {
  auto presence_scan_request = ::nearby::presence::ScanRequest();
  presence_scan_request.account_name = scan_request->account_name;
  for (auto identity_type : scan_request->identity_types) {
    presence_scan_request.identity_types.push_back(
        ConvertMojomIdentityType(identity_type));
  }
  uint64_t session_id;
  auto id = id_++;
  auto session_id_or_status = presence_client_->StartScan(
      presence_scan_request,
      {.start_scan_cb =
           [this, id](absl::Status status) {
             if (status.ok()) {
               std::move(session_id_to_results_callback_map_
                             [id_to_session_id_map_[id]])
                   .Run(std::move(session_id_to_scan_session_remote_map_
                                      [id_to_session_id_map_[id]]),
                        ConvertStatusToMojom(status));
             } else {
               std::move(session_id_to_results_callback_map_
                             [id_to_session_id_map_[id]])
                   .Run(mojo::NullRemote(), ConvertStatusToMojom(status));
               session_id_to_scan_session_remote_map_.erase(
                   id_to_session_id_map_[id]);
               id_to_session_id_map_.erase(id);
             }
           },
       .on_discovered_cb =
           [this](::nearby::presence::PresenceDevice device) {
             scan_observer_remote_->OnDeviceFound(
                 BuildPresenceMojomDevice(device));
           },
       .on_updated_cb =
           [this](::nearby::presence::PresenceDevice device) {
             scan_observer_remote_->OnDeviceChanged(
                 BuildPresenceMojomDevice(device));
           },
       .on_lost_cb =
           [this](::nearby::presence::PresenceDevice device) {
             scan_observer_remote_->OnDeviceLost(
                 BuildPresenceMojomDevice(device));
           }});

  if (session_id_or_status.ok()) {
    session_id = *session_id_or_status;
  } else {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << __func__ << ": Error starting scan, status was: "
        << session_id_or_status.status();
    std::move(callback).Run(
        std::move(mojo::NullRemote()),
        ConvertStatusToMojom(session_id_or_status.status()));
    return;
  }

  auto iter = session_id_to_scan_session_map_.emplace(
      session_id, std::make_unique<ScanSessionImpl>());
  auto* scan_session_impl = iter.first->second.get();

  mojo::PendingRemote<mojom::ScanSession> scan_session_remote =
      scan_session_impl->receiver.BindNewPipeAndPassRemote();
  scan_session_impl->receiver.set_disconnect_handler(
      base::BindOnce(&NearbyPresence::OnScanSessionDisconnect,
                     weak_ptr_factory_.GetWeakPtr(), session_id));
  session_id_to_scan_session_remote_map_.emplace(
      session_id, std::move(scan_session_remote));

  // When `callback` is invoked by the closure above, it will occur on a
  // different Sequence than the current Sequence. Wrap `callback` in a helper
  // function that will post `callback` back onto the current Sequence.
  session_id_to_results_callback_map_.emplace(
      session_id,
      base::BindOnce(&PostStartScanCallbackOnSequence, std::move(callback),
                     base::SequencedTaskRunner::GetCurrentDefault()));

  id_to_session_id_map_.insert_or_assign(id, session_id);
}

void NearbyPresence::UpdateLocalDeviceMetadata(mojom::MetadataPtr metadata) {
  // PresenceService exposes the same API to set local device metadata and
  // an optional field to generate credentials.
  // NearbyPresence::UpdateLocalDeviceMetadata only sets the local device
  // metadata, which is why `regen_credentials` is false. Similarly, since
  // there is no credentials being regenerated, an empty callback is passed to
  // `credentials_generated_cb`. The NP library requires calls on every
  // start up of CrOS Nearby Presence Service to set the device metadata, since
  // it is only stored in memory, and thus `UpdateLocalDeviceMetadata` is called
  // to only set metadata, and not to generate credentials. Generating
  // credentials is only called during the first time flow or when device
  // metadata changes (e.g. the user's name).
  presence_service_->UpdateDeviceIdentityMetaData(
      MetadataFromMojom(metadata.get()),
      /*regen_credentials=*/false,
      /*manager_app_id=*/kChromeOSManagerAppId,
      /*identity_types=*/
      {::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP},
      /*credential_life_cycle_days=*/kCredentialLifeCycleDays,
      /*contiguous_copy_of_credentials=*/kNumCredentials,
      /*credentials_generated_cb=*/{});
}

void NearbyPresence::UpdateLocalDeviceMetadataAndGenerateCredentials(
    mojom::MetadataPtr metadata,
    UpdateLocalDeviceMetadataAndGenerateCredentialsCallback callback) {
  presence_service_->UpdateDeviceIdentityMetaData(
      MetadataFromMojom(metadata.get()),
      /*regen_credentials=*/true,
      /*manager_app_id=*/kChromeOSManagerAppId,
      /*identity_types=*/
      {::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP},
      /*credential_life_cycle_days=*/kCredentialLifeCycleDays,
      /*contiguous_copy_of_credentials=*/kNumCredentials,
      {.credentials_generated_cb = [cb = base::BindOnce(std::move(callback)),
                                    task_runner = base::SequencedTaskRunner::
                                        GetCurrentDefault()](
                                       auto status_or_shared_credentials) {
        std::vector<mojom::SharedCredentialPtr> mojo_credentials;

        if (status_or_shared_credentials.ok()) {
          for (auto credential : status_or_shared_credentials.value()) {
            mojo_credentials.push_back(SharedCredentialToMojom(credential));
          }
        }

        // absl::AnyInvocable marks its bound parameters as const&, but
        // base::BindOnce() expects a non-const rvalue. Cast it as non-const to
        // allow the bind.
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(
                std::move(
                    const_cast<
                        UpdateLocalDeviceMetadataAndGenerateCredentialsCallback&>(
                        cb)),
                /*credentials=*/std::move(mojo_credentials), /*status=*/
                ConvertStatusToMojom(status_or_shared_credentials.status())));
      }});
}

void NearbyPresence::OnScanSessionDisconnect(uint64_t scan_session_id) {
  CD_LOG(VERBOSE, Feature::NEARBY_INFRA) << __func__;
  presence_client_->StopScan(scan_session_id);
  session_id_to_scan_session_map_.erase(scan_session_id);
  session_id_to_results_callback_map_.erase(scan_session_id);
  session_id_to_scan_session_remote_map_.erase(scan_session_id);
  auto iter = id_to_session_id_map_.begin();
  while (iter != id_to_session_id_map_.end()) {
    if (iter->second == scan_session_id) {
      id_to_session_id_map_.erase(iter);
      break;
    }
    iter++;
  }
}

void NearbyPresence::UpdateRemoteSharedCredentials(
    std::vector<mojom::SharedCredentialPtr> shared_credentials,
    const std::string& account_name,
    UpdateRemoteSharedCredentialsCallback callback) {
  std::vector<::nearby::internal::SharedCredential> proto_credentials;
  for (const auto& credential : shared_credentials) {
    proto_credentials.push_back(SharedCredentialFromMojom(credential.get()));
  }

  presence_service_->UpdateRemotePublicCredentials(
      /*manager_app_id=*/kChromeOSManagerAppId, account_name, proto_credentials,
      {.credentials_updated_cb =
           [cb = base::BindOnce(std::move(callback)),
            task_runner =
                base::SequencedTaskRunner::GetCurrentDefault()](auto status) {
             // absl::AnyInvocable marks its bound parameters as const&, but
             // base::BindOnce() expects a non-const rvalue. Cast it as
             // non-const to allow the bind.
             task_runner->PostTask(
                 FROM_HERE,
                 base::BindOnce(
                     std::move(
                         const_cast<UpdateRemoteSharedCredentialsCallback&>(
                             cb)),
                     ConvertStatusToMojom(status)));
           }});
}

void NearbyPresence::GetLocalSharedCredentials(
    const std::string& account_name,
    GetLocalSharedCredentialsCallback callback) {
  presence_service_->GetLocalPublicCredentials(
      /*credential_selector=*/{.manager_app_id = kChromeOSManagerAppId,
                               .account_name = account_name,
                               .identity_type = ::nearby::internal::
                                   IdentityType::IDENTITY_TYPE_PRIVATE_GROUP},
      {.credentials_fetched_cb =
           [cb = base::BindOnce(std::move(callback)),
            task_runner = base::SequencedTaskRunner::GetCurrentDefault()](
               auto status_or_shared_credentials) {
             std::vector<mojom::SharedCredentialPtr> mojo_credentials;

             if (status_or_shared_credentials.ok()) {
               for (auto credential : status_or_shared_credentials.value()) {
                 mojo_credentials.push_back(
                     SharedCredentialToMojom(credential));
               }
             }

             // absl::AnyInvocable marks its bound parameters as const&, but
             // base::BindOnce() expects a non-const rvalue. Cast it as
             // non-const to allow the bind.
             task_runner->PostTask(
                 FROM_HERE,
                 base::BindOnce(
                     std::move(
                         const_cast<GetLocalSharedCredentialsCallback&>(cb)),
                     /*credentials=*/std::move(mojo_credentials), /*status=*/
                     ConvertStatusToMojom(
                         status_or_shared_credentials.status())));
           }});
}

NearbyPresence::ScanSessionImpl::ScanSessionImpl() {}
NearbyPresence::ScanSessionImpl::~ScanSessionImpl() {}

}  // namespace ash::nearby::presence
