// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/exponential_moving_average.h"

#include <cmath>

#include "base/check.h"

namespace performance_manager {

ExponentialMovingAverage::ExponentialMovingAverage(float alpha)
    : alpha_(alpha) {
  DCHECK(alpha_ > 0.0 && alpha_ < 1.0);
}

void ExponentialMovingAverage::AppendDatum(float datum) {
  if (num_datums_ == 0) {
    first_datum_ = datum;
    value_ = datum;
  } else {
    value_ = value_ * (1.0 - alpha_) + datum * alpha_;
  }

  ++num_datums_;
}

void ExponentialMovingAverage::PrependDatum(float datum) {
  if (num_datums_ == 0) {
    first_datum_ = datum;
    value_ = datum;
  } else {
    // Back the previous first datum out of the value.
    float beta = 1.0 - alpha_;
    float betan = std::pow(1.0 - alpha_, num_datums_ - 1);
    float first_datum_contrib = first_datum_ * betan;
    float other_datums_contrib = value_ - first_datum_contrib;
    first_datum_ = datum;
    value_ = datum * betan * beta + first_datum_contrib * alpha_ +
             other_datums_contrib;
  }

  ++num_datums_;
}

void ExponentialMovingAverage::Clear() {
  first_datum_ = 0.0;
  value_ = 0.0;
  num_datums_ = 0;
}

}  // namespace performance_manager
