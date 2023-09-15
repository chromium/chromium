// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_FEATURES_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_FEATURES_H_

#include "base/feature_list.h"

namespace autofill::features {

BASE_DECLARE_FEATURE(kAndroidAutofillFormSubmissionCheckById);

BASE_DECLARE_FEATURE(kAndroidAutofillSupportVisibilityChanges);

BASE_DECLARE_FEATURE(kAndroidAutofillViewStructureWithFormHierarchyLayer);

}  // namespace autofill::features

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_FEATURES_H_
