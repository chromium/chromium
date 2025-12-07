// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_REQUESTS_H_
#define CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_REQUESTS_H_

#include <array>
#include <string>

#include "components/cbor/values.h"
#include "crypto/hash.h"
#include "quick_start_message.h"

namespace ash::quick_start::requests {

std::unique_ptr<QuickStartMessage> BuildBootstrapOptionsRequest();

std::unique_ptr<QuickStartMessage> BuildAssertionRequestMessage(
    base::span<const uint8_t, crypto::hash::kSha256Size> client_data_hash);

std::unique_ptr<QuickStartMessage> BuildGetInfoRequestMessage();

std::unique_ptr<QuickStartMessage> BuildRequestWifiCredentialsMessage(
    uint64_t session_id,
    std::string& shared_secret);

std::vector<uint8_t> CBOREncodeGetAssertionRequest(const cbor::Value& request);

cbor::Value GenerateGetAssertionRequest(
    base::span<const uint8_t, crypto::hash::kSha256Size> client_data_hash);

std::unique_ptr<QuickStartMessage> BuildNotifySourceOfUpdateMessage(
    uint64_t session_id,
    base::span<const uint8_t, 32> shared_secret);

std::unique_ptr<QuickStartMessage> BuildBootstrapStateCancelMessage();

std::unique_ptr<QuickStartMessage> BuildBootstrapStateCompleteMessage();

}  // namespace ash::quick_start::requests

#endif  // CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_REQUESTS_H_
