// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_CONVERSIONS_H_
#define CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_CONVERSIONS_H_

#include <cstdint>
#include <vector>

#include "base/callback_forward.h"
#include "chrome/services/sharing/public/mojom/nearby_connections.mojom-forward.h"
#include "chrome/services/sharing/public/mojom/nearby_connections_types.mojom-forward.h"
#include "third_party/nearby/src/cpp/core_v2/options.h"
#include "third_party/nearby/src/cpp/core_v2/params.h"

namespace location {
namespace nearby {
namespace connections {

using StatusCallback = base::OnceCallback<void(mojom::Status)>;

Strategy StrategyFromMojom(mojom::Strategy strategy);

mojom::Status StatusToMojom(Status::Value status);

ResultCallback ResultCallbackFromMojom(StatusCallback callback);

std::vector<uint8_t> ByteArrayToMojom(const ByteArray& byte_array);

ByteArray ByteArrayFromMojom(const std::vector<uint8_t>& byte_array);

mojom::PayloadStatus PayloadStatusToMojom(PayloadProgressInfo::Status status);

mojom::Medium MediumToMojom(Medium medium);

BooleanMediumSelector MediumSelectorFromMojom(
    mojom::MediumSelection* allowed_mediums);

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_CONVERSIONS_H_
