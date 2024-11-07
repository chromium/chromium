// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/precondition_list_provider.h"

#include "components/user_education/common/feature_promo/feature_promo_precondition.h"

namespace user_education {

ComposingPreconditionListProvider::ComposingPreconditionListProvider() =
    default;
ComposingPreconditionListProvider::~ComposingPreconditionListProvider() =
    default;

void ComposingPreconditionListProvider::AddProvider(
    const PreconditionListProvider* provider) {
  providers_.emplace_back(provider);
}

FeaturePromoPreconditionList
ComposingPreconditionListProvider::GetPreconditions(
    const FeaturePromoSpecification& spec) const {
  FeaturePromoPreconditionList result;
  for (const auto& provider : providers_) {
    result.AppendAll(provider->GetPreconditions(spec));
  }
  return result;
}

}  // namespace user_education
