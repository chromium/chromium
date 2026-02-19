// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_COMMON_AUTOFILL_OPTIMIZATION_FEATURES_H_
#define COMPONENTS_AUTOFILL_IOS_COMMON_AUTOFILL_OPTIMIZATION_FEATURES_H_

#include "base/feature_list.h"

namespace autofill::features {

// Improves the form extraction performance by 2x by optimizing the traversal
// and processing of form elements. The feature avoids interacting with
// the full DOM (document.all, getElementsByTagName) and instead works with a
// specific HTML element subset. It also avoids memory allocations by using a
// single loop instead of chaining array methods.
BASE_DECLARE_FEATURE(kAutofillOptimizationFormSearchIos);

}  // namespace autofill::features

#endif  // COMPONENTS_AUTOFILL_IOS_COMMON_AUTOFILL_OPTIMIZATION_FEATURES_H_
