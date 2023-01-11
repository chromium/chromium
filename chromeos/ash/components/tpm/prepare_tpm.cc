// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tpm/prepare_tpm.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"

namespace ash {

namespace {

void OnClearStoredOwnerPassword(
    base::OnceClosure preparation_finished_callback,
    const ::tpm_manager::ClearStoredOwnerPasswordReply& reply) {
  LOG_IF(ERROR, reply.status() != ::tpm_manager::STATUS_SUCCESS)
      << "Failed to call ClearStoredOwnerPassword; status: " << reply.status();
  std::move(preparation_finished_callback).Run();
}

void OnTakeOwnership(base::OnceClosure preparation_finished_callback,
                     const ::tpm_manager::TakeOwnershipReply& reply) {
  LOG_IF(ERROR, reply.status() != ::tpm_manager::STATUS_SUCCESS)
      << "Failed to call TakeOwnership; status: " << reply.status();
  std::move(preparation_finished_callback).Run();
}

void OnGetTpmStatus(base::OnceClosure preparation_finished_callback,
                    const ::tpm_manager::GetTpmNonsensitiveStatusReply& reply) {
  if (reply.status() != ::tpm_manager::STATUS_SUCCESS) {
    LOG(WARNING) << " Failed to get tpm status; status: " << reply.status();
  }
  if (reply.status() != ::tpm_manager::STATUS_SUCCESS || !reply.is_enabled()) {
    LOG_IF(WARNING, !reply.is_enabled()) << "TPM is reportedly disabled.";
    std::move(preparation_finished_callback).Run();
    return;
  }
  if (reply.is_owned()) {
    chromeos::TpmManagerClient::Get()->ClearStoredOwnerPassword(
        ::tpm_manager::ClearStoredOwnerPasswordRequest(),
        base::BindOnce(OnClearStoredOwnerPassword,
                       std::move(preparation_finished_callback)));
  } else {
    chromeos::TpmManagerClient::Get()->TakeOwnership(
        ::tpm_manager::TakeOwnershipRequest(),
        base::BindOnce(OnTakeOwnership,
                       std::move(preparation_finished_callback)));
  }
}

}  // namespace

void PrepareTpm(base::OnceClosure preparation_finished_callback) {
  chromeos::TpmManagerClient::Get()->GetTpmNonsensitiveStatus(
      ::tpm_manager::GetTpmNonsensitiveStatusRequest(),
      base::BindOnce(OnGetTpmStatus, std::move(preparation_finished_callback)));
}

}  // namespace ash
