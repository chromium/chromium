// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/cors_exempt_headers.h"

#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"

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

bool IsCorsExemptHeader(std::string_view header) {
  static const base::NoDestructor<base::flat_set<std::string>>
      exempt_header_set(kExemptHeaders,
                        kExemptHeaders + std::size(kExemptHeaders));
  return exempt_header_set->find(header) != exempt_header_set->end();
}

}  // namespace chromecast
