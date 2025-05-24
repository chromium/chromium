// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/precondition_list_provider.h"

#include "components/user_education/common/feature_promo/feature_promo_precondition.h"

namespace user_education {

CallbackPreconditionListProvider::CallbackPreconditionListProvider(
    Callback callback)
    : callback_(std::move(callback)) {}
CallbackPreconditionListProvider::~CallbackPreconditionListProvider() = default;

FeaturePromoPreconditionList CallbackPreconditionListProvider::GetPreconditions(
    const FeaturePromoSpecification& spec,
    const FeaturePromoParams& params) const {
  return callback_.Run(spec, params);
}

ComposingPreconditionListProvider::ComposingPreconditionListProvider() =
    default;
ComposingPreconditionListProvider::~ComposingPreconditionListProvider() =
    default;

void ComposingPreconditionListProvider::AddProvider(
    const PreconditionListProvider* provider) {
  providers_.emplace_back(provider);
}

void ComposingPreconditionListProvider::AddProvider(
    PreconditionListProviderCallback callback) {
  const auto& result = callback_providers_.emplace_back(
      std::make_unique<CallbackPreconditionListProvider>(std::move(callback)));
  AddProvider(result.get());
}

FeaturePromoPreconditionList
ComposingPreconditionListProvider::GetPreconditions(
    const FeaturePromoSpecification& spec,
    const FeaturePromoParams& params) const {
  FeaturePromoPreconditionList result;
  for (const auto& provider : providers_) {
    result.AppendAll(provider->GetPreconditions(spec, params));
  }
  return result;
}

}  // namespace user_education
