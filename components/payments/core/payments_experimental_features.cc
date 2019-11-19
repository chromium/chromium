// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payments_experimental_features.h"

#include "base/feature_list.h"
#include "components/payments/core/features.h"

namespace payments {

// static
bool PaymentsExperimentalFeatures::IsEnabled(const base::Feature& feature) {
  return base::FeatureList::IsEnabled(
             features::kWebPaymentsExperimentalFeatures) ||
         base::FeatureList::IsEnabled(feature);
}

}  // namespace payments
