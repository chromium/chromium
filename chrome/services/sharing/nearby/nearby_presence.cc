// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_presence.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/services/sharing/nearby/nearby_shared_remotes.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/nearby/src/presence/presence_service.h"

namespace {

void PostStartScanCallbackOnSequence(
    ash::nearby::presence::NearbyPresence::StartScanCallback callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<ash::nearby::presence::mojom::ScanSession> scan_session,
    ash::nearby::presence::mojom::StatusCode status) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(scan_session), status));
}

ash::nearby::presence::mojom::StatusCode CovertStatusToMojomStatus(
    absl::Status status) {
  if (status.code() == absl::StatusCode::kOk) {
    return ash::nearby::presence::mojom::StatusCode::kOk;
  } else {
    return ash::nearby::presence::mojom::StatusCode::kFailure;
  }
}

}  // namespace

namespace ash::nearby::presence {

NearbyPresence::NearbyPresence(
    mojo::PendingReceiver<mojom::NearbyPresence> nearby_presence,
    base::OnceClosure on_disconnect)
    : presence_service_(::nearby::presence::PresenceService()),
      presence_client_(presence_service_.CreatePresenceClient()),
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
  presence_scan_request.identity_types.push_back(
      ::nearby::internal::IdentityType::IDENTITY_TYPE_PUBLIC);
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
                        CovertStatusToMojomStatus(status));
             } else {
               std::move(session_id_to_results_callback_map_
                             [id_to_session_id_map_[id]])
                   .Run(mojo::NullRemote(), CovertStatusToMojomStatus(status));
               session_id_to_scan_session_remote_map_.erase(
                   id_to_session_id_map_[id]);
               id_to_session_id_map_.erase(id);
             }
           },
       .on_discovered_cb =
           [this](::nearby::presence::PresenceDevice device) {
             // TODO(b/276642472): Properly plumb type and stable_device_id.
             scan_observer_remote_->OnDeviceFound(mojom::PresenceDevice::New(
                 device.GetEndpointId(), device.GetMetadata().device_name(),
                 mojom::PresenceDeviceType::kPhone,
                 /*stable_device_id=*/absl::nullopt));
           },
       .on_updated_cb =
           [this](::nearby::presence::PresenceDevice device) {
             // TODO(b/276642472): Properly plumb type and stable_device_id.
             scan_observer_remote_->OnDeviceChanged(mojom::PresenceDevice::New(
                 device.GetEndpointId(), device.GetMetadata().device_name(),
                 mojom::PresenceDeviceType::kPhone,
                 /*stable_device_id=*/absl::nullopt));
           },
       .on_lost_cb =
           [this](::nearby::presence::PresenceDevice device) {
             // TODO(b/276642472): Properly plumb type and stable_device_id.
             scan_observer_remote_->OnDeviceLost(mojom::PresenceDevice::New(
                 device.GetEndpointId(), device.GetMetadata().device_name(),
                 mojom::PresenceDeviceType::kPhone,
                 /*stable_device_id=*/absl::nullopt));
           }});

  if (session_id_or_status.ok()) {
    session_id = *session_id_or_status;
  } else {
    // TODO(b/277819923): Change logging to presence specific logs.
    NS_LOG(ERROR) << __func__ << ": Error starting scan, status was: "
                  << session_id_or_status.status();
    std::move(callback).Run(
        std::move(mojo::NullRemote()),
        CovertStatusToMojomStatus(session_id_or_status.status()));
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

void NearbyPresence::OnScanSessionDisconnect(uint64_t scan_session_id) {
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

NearbyPresence::ScanSessionImpl::ScanSessionImpl() {}
NearbyPresence::ScanSessionImpl::~ScanSessionImpl() {}

}  // namespace ash::nearby::presence
