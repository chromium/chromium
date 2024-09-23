// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/quick_pair_process.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "chromeos/ash/services/quick_pair/public/cpp/not_discoverable_advertisement.h"
#include "chromeos/ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "components/cross_device/logging/logging.h"

namespace ash {
namespace quick_pair {
namespace quick_pair_process {

namespace {

QuickPairProcessManager* g_process_manager = nullptr;

std::unique_ptr<QuickPairProcessManager::ProcessReference> GetProcessReference(
    ProcessStoppedCallback process_stopped_callback) {
  if (!g_process_manager) {
    CD_LOG(ERROR, Feature::FP)
        << "QuickPairProcess::SetProcessManager() must be called "
           "before any QuickPairProcess use.";
    return nullptr;
  }

  return g_process_manager->GetProcessReference(
      std::move(process_stopped_callback));
}

}  // namespace

void SetProcessManager(QuickPairProcessManager* process_manager) {
  g_process_manager = process_manager;
}

void GetHexModelIdFromServiceData(
    const std::vector<uint8_t>& service_data,
    GetHexModelIdFromServiceDataCallback callback,
    ProcessStoppedCallback process_stopped_callback) {
  std::unique_ptr<QuickPairProcessManager::ProcessReference> process_reference =
      GetProcessReference(std::move(process_stopped_callback));

  if (!process_reference) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Failed to get new process reference.";
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto* raw_process_reference = process_reference.get();

  raw_process_reference->GetFastPairDataParser()->GetHexModelIdFromServiceData(
      service_data,
      base::BindOnce(
          [](std::unique_ptr<QuickPairProcessManager::ProcessReference>,
             GetHexModelIdFromServiceDataCallback callback,
             const std::optional<std::string>& result) {
            std::move(callback).Run(result);
          },
          std::move(process_reference), std::move(callback)));
}

void ParseDecryptedResponse(
    const std::vector<uint8_t>& aes_key,
    const std::vector<uint8_t>& encrypted_response_bytes,
    ParseDecryptedResponseCallback callback,
    ProcessStoppedCallback process_stopped_callback) {
  std::unique_ptr<QuickPairProcessManager::ProcessReference> process_reference =
      GetProcessReference(std::move(process_stopped_callback));

  if (!process_reference) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Failed to get new process reference.";
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto* raw_process_reference = process_reference.get();

  raw_process_reference->GetFastPairDataParser()->ParseDecryptedResponse(
      aes_key, encrypted_response_bytes,
      base::BindOnce(
          [](std::unique_ptr<QuickPairProcessManager::ProcessReference>,
             ParseDecryptedResponseCallback callback,
             const std::optional<DecryptedResponse>& result) {
            std::move(callback).Run(result);
          },
          std::move(process_reference), std::move(callback)));
}

void ParseDecryptedPasskey(const std::vector<uint8_t>& aes_key,
                           const std::vector<uint8_t>& encrypted_passkey_bytes,
                           ParseDecryptedPasskeyCallback callback,
                           ProcessStoppedCallback process_stopped_callback) {
  std::unique_ptr<QuickPairProcessManager::ProcessReference> process_reference =
      GetProcessReference(std::move(process_stopped_callback));

  if (!process_reference) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Failed to get new process reference.";
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto* raw_process_reference = process_reference.get();

  raw_process_reference->GetFastPairDataParser()->ParseDecryptedPasskey(
      aes_key, encrypted_passkey_bytes,
      base::BindOnce(
          [](std::unique_ptr<QuickPairProcessManager::ProcessReference>,
             ParseDecryptedPasskeyCallback callback,
             const std::optional<DecryptedPasskey>& result) {
            std::move(callback).Run(result);
          },
          std::move(process_reference), std::move(callback)));
}

void ParseNotDiscoverableAdvertisement(
    const std::vector<uint8_t>& service_data,
    const std::string& address,
    ParseNotDiscoverableAdvertisementCallback callback,
    ProcessStoppedCallback process_stopped_callback) {
  std::unique_ptr<QuickPairProcessManager::ProcessReference> process_reference =
      GetProcessReference(std::move(process_stopped_callback));

  if (!process_reference) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Failed to get new process reference.";
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto* raw_process_reference = process_reference.get();

  raw_process_reference->GetFastPairDataParser()
      ->ParseNotDiscoverableAdvertisement(
          service_data, address,
          base::BindOnce(
              [](std::unique_ptr<QuickPairProcessManager::ProcessReference>,
                 ParseNotDiscoverableAdvertisementCallback callback,
                 const std::optional<NotDiscoverableAdvertisement>& result) {
                std::move(callback).Run(result);
              },
              std::move(process_reference), std::move(callback)));
}

void ParseMessageStreamMessages(
    const std::vector<uint8_t>& message_bytes,
    ParseMessageStreamMessagesCallback callback,
    ProcessStoppedCallback process_stopped_callback) {
  std::unique_ptr<QuickPairProcessManager::ProcessReference> process_reference =
      GetProcessReference(std::move(process_stopped_callback));

  if (!process_reference) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Failed to get new process reference.";
    std::move(callback).Run({});
    return;
  }

  auto* raw_process_reference = process_reference.get();

  raw_process_reference->GetFastPairDataParser()->ParseMessageStreamMessages(
      message_bytes,
      base::BindOnce(
          [](std::unique_ptr<QuickPairProcessManager::ProcessReference>,
             ParseMessageStreamMessagesCallback callback,
             std::vector<mojom::MessageStreamMessagePtr> result) {
            std::move(callback).Run(std::move(result));
          },
          std::move(process_reference), std::move(callback)));
}

}  // namespace quick_pair_process

}  // namespace quick_pair
}  // namespace ash
