// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_FEATURES_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_FEATURES_H_

#include "base/feature_list.h"

namespace autofill::features {

BASE_DECLARE_FEATURE(kAutofillVirtualViewStructureAndroidPasskeyLongPress);

BASE_DECLARE_FEATURE(kAndroidAutofillLazyFrameworkWrapper);

BASE_DECLARE_FEATURE(kAndroidAutofillForwardIframeOrigin);

BASE_DECLARE_FEATURE(kAndroidAutofillImprovedVisibilityDetection);

BASE_DECLARE_FEATURE(kAndroidAutofillUpdateContextForWebContents);

BASE_DECLARE_FEATURE(kAndroidAutofillSupportForHttpAuth);

}  // namespace autofill::features

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_FEATURES_H_
