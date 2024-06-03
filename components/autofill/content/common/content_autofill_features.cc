// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/common/content_autofill_features.h"

namespace autofill::features {

// If enabled, we will store autofill server card data in shared storage.
BASE_FEATURE(kAutofillSharedStorageServerCardData,
             "AutofillSharedStorageServerCardData",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, we exclude geolocation data in the risk fingerprint.
BASE_FEATURE(kAutofillDisableGeolocationInRiskFingerprint,
             "AutofillDisableGeolocationInRiskFingerprint",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace autofill::features
