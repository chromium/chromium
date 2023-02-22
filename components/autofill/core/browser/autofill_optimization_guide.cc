// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_optimization_guide.h"

#include "components/optimization_guide/core/new_optimization_guide_decider.h"

namespace autofill {

AutofillOptimizationGuide::AutofillOptimizationGuide(
    optimization_guide::NewOptimizationGuideDecider* decider)
    : decider_(decider) {}

AutofillOptimizationGuide::~AutofillOptimizationGuide() = default;

}  // namespace autofill
