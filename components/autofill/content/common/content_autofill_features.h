// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_COMMON_CONTENT_AUTOFILL_FEATURES_H_
#define COMPONENTS_AUTOFILL_CONTENT_COMMON_CONTENT_AUTOFILL_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace autofill::features {

COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillSharedStorageServerCardData);

COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDisableGeolocationInRiskFingerprint);

}  // namespace autofill::features

#endif  // COMPONENTS_AUTOFILL_CONTENT_COMMON_CONTENT_AUTOFILL_FEATURES_H_
