// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/optimization_guide/mock_autofill_optimization_guide_decider.h"

namespace autofill {

MockAutofillOptimizationGuideDecider::MockAutofillOptimizationGuideDecider()
    : AutofillOptimizationGuideDecider(nullptr) {}
MockAutofillOptimizationGuideDecider::~MockAutofillOptimizationGuideDecider() =
    default;

}  // namespace autofill
