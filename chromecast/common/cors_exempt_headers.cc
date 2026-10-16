// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/cors_exempt_headers.h"

#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"

namespace chromecast {
namespace {

constexpr auto kExemptHeaders = base::MakeFixedFlatSet<std::string_view>(
    base::sorted_unique,
    {
        // keep-sorted start
        "Authorization",
        "CAST-APP-DEVICE-ID",
        "CAST-APP-ID",
        "CAST-CERT",
        "CAST-CERT-CHAIN",
        "CAST-CKP-MODULUS",
        "CAST-CKP-PUBLIC-EXPONENT",
        "CAST-DEVICE-CAPABILITIES",
        "CAST-DEVICE-IN-MANAGED-MODE",
        "CAST-JWT",
        "CAST-PRIVATE-DATA",
        "CAST-PROD",
        "CAST-SERIAL-NUM",
        "CAST-SESSION-ID",
        "CAST-SIGNATURE",
        "CAST-SIGNATURE-TIME",
        "X-Home-DeviceLinkedUserCreds",
        "google_cast_bg",
        // keep-sorted end

        // This header list is for legacy compatibility only. Do not add any
        // more entries.
    });

}  // namespace

base::span<const std::string_view> GetLegacyCorsExemptHeaders() {
  return base::span<const std::string_view>(kExemptHeaders);
}

bool IsCorsExemptHeader(std::string_view header) {
  return kExemptHeaders.contains(header);
}

}  // namespace chromecast
