// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENTS_EXPERIMENTAL_FEATURES_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENTS_EXPERIMENTAL_FEATURES_H_

#include "base/macros.h"

namespace base {
struct Feature;
}  // namespace base

namespace payments {

class PaymentsExperimentalFeatures {
 public:
  // Utility wrapper around base::FeatureList::IsEnabled(). Returns true if
  // either |feature| or payments::features::kWebPaymentsExperimentalFeatures is
  // enabled.
  static bool IsEnabled(const base::Feature& feature);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PaymentsExperimentalFeatures);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENTS_EXPERIMENTAL_FEATURES_H_
