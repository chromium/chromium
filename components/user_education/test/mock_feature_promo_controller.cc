// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/mock_feature_promo_controller.h"

namespace user_education::test {

MockFeaturePromoController::MockFeaturePromoController() = default;
MockFeaturePromoController::~MockFeaturePromoController() = default;

base::WeakPtr<FeaturePromoController>
MockFeaturePromoController::GetAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace user_education::test
