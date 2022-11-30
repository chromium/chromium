// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENTS_EXPERIMENTAL_FEATURES_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENTS_EXPERIMENTAL_FEATURES_H_

#include "base/feature_list.h"

namespace payments {

class PaymentsExperimentalFeatures {
 public:
  PaymentsExperimentalFeatures() = delete;
  PaymentsExperimentalFeatures(const PaymentsExperimentalFeatures&) = delete;
  PaymentsExperimentalFeatures& operator=(const PaymentsExperimentalFeatures&) =
      delete;

  // Utility wrapper around base::FeatureList::IsEnabled(). Returns true if
  // either |feature| or payments::features::kWebPaymentsExperimentalFeatures is
  // enabled.
  static bool IsEnabled(const base::Feature& feature);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENTS_EXPERIMENTAL_FEATURES_H_
