// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_data_remover.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"

namespace arc {
namespace {

chromeos::ConciergeClient* GetConciergeClient() {
  return chromeos::DBusThreadManager::Get()->GetConciergeClient();
}

}  // namespace

// The conversion of upstart job names to dbus object paths is undocumented. See
// function nih_dbus_path in libnih for the implementation.
constexpr char kArcRemoveDataUpstartJob[] = "arc_2dremove_2ddata";

ArcDataRemover::ArcDataRemover(PrefService* prefs,
                               const cryptohome::Identification& cryptohome_id)
    : cryptohome_id_(cryptohome_id) {
  pref_.Init(prefs::kArcDataRemoveRequested, prefs);
}

ArcDataRemover::~ArcDataRemover() = default;

void ArcDataRemover::Schedule() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  pref_.SetValue(true);
}

bool ArcDataRemover::IsScheduledForTesting() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return pref_.GetValue();
}

void ArcDataRemover::Run(RunCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!pref_.GetValue()) {
    // Data removal is not scheduled.
    std::move(callback).Run(base::nullopt);
    return;
  }

  // TODO(yusukes): Stop special-casing ARCVM once we use virtio-fs for
  // exporting /data.
  if (IsArcVmEnabled()) {
    VLOG(1) << "Starting ARCVM data removal";
    chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->StartConcierge(
        base::BindOnce(&ArcDataRemover::OnConciergeStarted,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  VLOG(1) << "Starting ARC data removal";
  auto* upstart_client = chromeos::UpstartClient::Get();
  if (!upstart_client) {
    // May be null in tests
    std::move(callback).Run(base::nullopt);
    return;
  }
  const std::string account_id =
      cryptohome::CreateAccountIdentifierFromIdentification(cryptohome_id_)
          .account_id();
  upstart_client->StartJob(
      kArcRemoveDataUpstartJob, {"CHROMEOS_USER=" + account_id},
      base::AdaptCallbackForRepeating(
          base::BindOnce(&ArcDataRemover::OnDataRemoved,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

void ArcDataRemover::OnDataRemoved(RunCallback callback, bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (success) {
    VLOG(1) << "ARC data removal successful";
  } else {
    LOG(ERROR) << "Request for ARC user data removal failed. "
               << "See upstart logs for more details.";
  }
  pref_.SetValue(false);

  std::move(callback).Run(success);
}

void ArcDataRemover::OnConciergeStarted(RunCallback callback, bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to start Concierge service for arcvm";
    OnDataRemoved(std::move(callback), false);
    return;
  }
  vm_tools::concierge::DestroyDiskImageRequest request;
  request.set_cryptohome_id(user_id_hash_);
  request.set_disk_path(kArcVmName);
  GetConciergeClient()->DestroyDiskImage(
      std::move(request),
      base::BindOnce(&ArcDataRemover::OnDiskImageDestroyed,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcDataRemover::OnDiskImageDestroyed(
    RunCallback callback,
    base::Optional<vm_tools::concierge::DestroyDiskImageResponse> reply) {
  if (!reply) {
    LOG(ERROR) << "Failed to destroy disk image. Empty response.";
    OnDataRemoved(std::move(callback), false);
    return;
  }
  if (reply->status() != vm_tools::concierge::DISK_STATUS_DESTROYED) {
    LOG(ERROR) << "Failed to destroy disk image: " << reply->failure_reason();
    OnDataRemoved(std::move(callback), false);
    return;
  }
  OnDataRemoved(std::move(callback), true);
}

}  // namespace arc
