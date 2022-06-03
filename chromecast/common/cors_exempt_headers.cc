// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/cors_exempt_headers.h"

namespace chromecast {
namespace {

const char* kExemptHeaders[] = {
    "google_cast_bg",
    "CAST-DEVICE-CAPABILITIES",
    "CAST-SERIAL-NUM",
    "CAST-CERT",
    "CAST-CERT-CHAIN",
    "CAST-SIGNATURE",
    "CAST-SIGNATURE-TIME",
    "CAST-APP-ID",
    "CAST-SESSION-ID",
    "CAST-APP-DEVICE-ID",
    "CAST-PROD",
    "CAST-JWT",
    "CAST-PRIVATE-DATA",
    "CAST-CKP-MODULUS",
    "CAST-CKP-PUBLIC-EXPONENT",
    "Authorization",
    "X-Home-DeviceLinkedUserCreds",
    "CAST-DEVICE-IN-MANAGED-MODE",

    // This header list is for legacy compatibility only. Do not add any more
    // entries.
};

}  // namespace

base::span<const char*> GetLegacyCorsExemptHeaders() {
  return base::span<const char*>(kExemptHeaders);
}

}  // namespace chromecast
