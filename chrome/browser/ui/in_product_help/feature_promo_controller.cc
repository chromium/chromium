// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/in_product_help/feature_promo_controller.h"

FeaturePromoController::PromoHandle::PromoHandle(
    base::WeakPtr<FeaturePromoController> controller)
    : controller_(std::move(controller)) {}

FeaturePromoController::PromoHandle::~PromoHandle() {
  if (controller_)
    controller_->FinishContinuedPromo();
}

FeaturePromoController::PromoHandle::PromoHandle(PromoHandle&& other) = default;

FeaturePromoController::PromoHandle&
FeaturePromoController::PromoHandle::operator=(PromoHandle&& other) = default;
