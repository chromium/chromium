// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_QUICK_PAIR_QUICK_PAIR_PROCESS_H_
#define CHROMEOS_ASH_SERVICES_QUICK_PAIR_QUICK_PAIR_PROCESS_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"

namespace ash {
namespace quick_pair {

struct DecryptedResponse;
struct DecryptedPasskey;
struct NotDiscoverableAdvertisement;

namespace quick_pair_process {

void SetProcessManager(QuickPairProcessManager* process_manager);

using ProcessStoppedCallback =
    base::OnceCallback<void(QuickPairProcessManager::ShutdownReason)>;

using GetHexModelIdFromServiceDataCallback =
    base::OnceCallback<void(const std::optional<std::string>&)>;

void GetHexModelIdFromServiceData(
    const std::vector<uint8_t>& service_data,
    GetHexModelIdFromServiceDataCallback callback,
    ProcessStoppedCallback process_stopped_callback);

using ParseDecryptedResponseCallback =
    base::OnceCallback<void(const std::optional<DecryptedResponse>&)>;

void ParseDecryptedResponse(
    const std::vector<uint8_t>& aes_key,
    const std::vector<uint8_t>& encrypted_response_bytes,
    ParseDecryptedResponseCallback callback,
    ProcessStoppedCallback process_stopped_callback);

using ParseDecryptedPasskeyCallback =
    base::OnceCallback<void(const std::optional<DecryptedPasskey>&)>;

void ParseDecryptedPasskey(const std::vector<uint8_t>& aes_key,
                           const std::vector<uint8_t>& encrypted_passkey_bytes,
                           ParseDecryptedPasskeyCallback callback,
                           ProcessStoppedCallback process_stopped_callback);

using ParseNotDiscoverableAdvertisementCallback = base::OnceCallback<void(
    const std::optional<NotDiscoverableAdvertisement>&)>;

void ParseNotDiscoverableAdvertisement(
    const std::vector<uint8_t>& service_data,
    const std::string& address,
    ParseNotDiscoverableAdvertisementCallback callback,
    ProcessStoppedCallback process_stopped_callback);

using ParseMessageStreamMessagesCallback =
    base::OnceCallback<void(std::vector<mojom::MessageStreamMessagePtr>)>;

void ParseMessageStreamMessages(
    const std::vector<uint8_t>& message_bytes,
    ParseMessageStreamMessagesCallback callback,
    ProcessStoppedCallback process_stopped_callback);

}  // namespace quick_pair_process

}  // namespace quick_pair
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_QUICK_PAIR_QUICK_PAIR_PROCESS_H_
