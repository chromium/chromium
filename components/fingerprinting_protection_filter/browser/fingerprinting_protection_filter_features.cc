// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_filter_features.h"

namespace fingerprinting_protection_filter::features {

// When enabled, loads the Fingerprinting Protection component and evaluates
// resource requests on certain pages against the Fingerprinting Protection
// blocklist, possibly blocks via a subresource filter.
BASE_FEATURE(kEnableFingerprintingProtectionFilter,
             "EnableFingerprintingProtectionFilter",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace fingerprinting_protection_filter::features
