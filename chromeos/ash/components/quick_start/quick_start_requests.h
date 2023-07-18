// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_REQUESTS_H_
#define CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_REQUESTS_H_

#include <array>
#include <string>

#include "base/values.h"
#include "chromeos/ash/components/quick_start/types.h"
#include "components/cbor/values.h"
#include "quick_start_message.h"
#include "url/origin.h"

namespace ash::quick_start::requests {

std::unique_ptr<QuickStartMessage> BuildBootstrapOptionsRequest();

std::unique_ptr<QuickStartMessage> BuildAssertionRequestMessage(
    const Base64UrlString& challenge);

std::unique_ptr<QuickStartMessage> BuildGetInfoRequestMessage();

std::unique_ptr<QuickStartMessage> BuildRequestWifiCredentialsMessage(
    int32_t session_id,
    std::string& shared_secret);

std::vector<uint8_t> CBOREncodeGetAssertionRequest(const cbor::Value& request);

std::string CreateFidoClientDataJson(const url::Origin& origin,
                                     const Base64UrlString& challenge);

cbor::Value GenerateGetAssertionRequest(const Base64UrlString& challenge);

std::unique_ptr<QuickStartMessage> BuildNotifySourceOfUpdateMessage(
    int32_t session_id,
    const base::span<uint8_t, 32> shared_secret);
}  // namespace ash::quick_start::requests

#endif  // CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_REQUESTS_H
