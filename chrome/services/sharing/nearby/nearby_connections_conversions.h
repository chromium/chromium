// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_CONVERSIONS_H_
#define CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_CONVERSIONS_H_

#include <cstdint>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections.mojom-forward.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom-forward.h"
#include "third_party/nearby/src/connections/params.h"
#include "third_party/nearby/src/connections/v3/bandwidth_info.h"
#include "third_party/nearby/src/internal/interop/authentication_status.h"

namespace nearby::connections {

using StatusCallback = base::OnceCallback<void(mojom::Status)>;

Strategy StrategyFromMojom(mojom::Strategy strategy);

mojom::Status StatusToMojom(Status::Value status);

ResultCallback ResultCallbackFromMojom(StatusCallback callback);

std::vector<uint8_t> ByteArrayToMojom(const ByteArray& byte_array);

ByteArray ByteArrayFromMojom(const std::vector<uint8_t>& byte_array);

mojom::PayloadStatus PayloadStatusToMojom(PayloadProgressInfo::Status status);

mojom::Medium MediumToMojom(Medium medium);

mojom::BandwidthQuality BandwidthQualityToMojom(v3::Quality quality);

BooleanMediumSelector MediumSelectorFromMojom(
    mojom::MediumSelection* allowed_mediums);

mojom::AuthenticationStatus AuthenticationStatusToMojom(
    AuthenticationStatus status);

}  // namespace nearby::connections

#endif  // CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_CONVERSIONS_H_
