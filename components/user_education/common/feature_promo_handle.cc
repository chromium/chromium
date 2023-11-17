// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_handle.h"

#include "components/user_education/common/feature_promo_controller.h"

namespace user_education {

FeaturePromoHandle::FeaturePromoHandle() = default;

FeaturePromoHandle::FeaturePromoHandle(
    base::WeakPtr<FeaturePromoController> controller,
    const base::Feature* feature)
    : controller_(std::move(controller)), feature_(feature) {
  DCHECK(feature_);
}

FeaturePromoHandle::FeaturePromoHandle(FeaturePromoHandle&& other) noexcept
    : controller_(std::move(other.controller_)),
      feature_(std::exchange(other.feature_, nullptr)) {}

FeaturePromoHandle& FeaturePromoHandle::operator=(
    FeaturePromoHandle&& other) noexcept {
  if (this != &other) {
    Release();
    controller_ = std::move(other.controller_);
    feature_ = std::exchange(other.feature_, nullptr);
  }

  return *this;
}

FeaturePromoHandle::~FeaturePromoHandle() {
  Release();
}

void FeaturePromoHandle::Release() {
  if (controller_)
    controller_->FinishContinuedPromo(*feature_);
  controller_.reset();
  feature_ = nullptr;
}

}  // namespace user_education
