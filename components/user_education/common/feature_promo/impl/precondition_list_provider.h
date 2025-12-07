// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_PRECONDITION_LIST_PROVIDER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_PRECONDITION_LIST_PROVIDER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/user_education_context.h"

namespace user_education {

using PreconditionListProviderCallback =
    base::RepeatingCallback<FeaturePromoPreconditionList(
        const FeaturePromoSpecification&,
        const FeaturePromoParams&,
        const UserEducationContextPtr&)>;

// Factory for precondition lists for different types of feature promos.
class PreconditionListProvider {
 public:
  PreconditionListProvider() = default;
  PreconditionListProvider(const PreconditionListProvider&) = delete;
  void operator=(const PreconditionListProvider&) = delete;
  virtual ~PreconditionListProvider() = default;

  // Returns the set of preconditions that must be satisfied for the promo
  // defined by `spec` to be queued or shown.
  virtual FeaturePromoPreconditionList GetPreconditions(
      const FeaturePromoSpecification& spec,
      const FeaturePromoParams& params,
      const UserEducationContextPtr& context) const = 0;
};

// Factory for precondition lists that delegates creation of the list to a
// callback.
class CallbackPreconditionListProvider : public PreconditionListProvider {
 public:
  using Callback = PreconditionListProviderCallback;

  explicit CallbackPreconditionListProvider(Callback callback);
  ~CallbackPreconditionListProvider() override;

  FeaturePromoPreconditionList GetPreconditions(
      const FeaturePromoSpecification& spec,
      const FeaturePromoParams& params,
      const UserEducationContextPtr& context) const override;

 private:
  Callback callback_;
};

// Factory that returns a combination of preconditions from other providers.
class ComposingPreconditionListProvider : public PreconditionListProvider {
 public:
  ComposingPreconditionListProvider();
  ~ComposingPreconditionListProvider() override;

  void AddProvider(const PreconditionListProvider* provider);
  void AddProvider(PreconditionListProviderCallback callback);

  // PreconditionListProvider:
  FeaturePromoPreconditionList GetPreconditions(
      const FeaturePromoSpecification& spec,
      const FeaturePromoParams& params,
      const UserEducationContextPtr& context) const override;

 private:
  std::vector<std::unique_ptr<CallbackPreconditionListProvider>>
      callback_providers_;
  std::vector<raw_ptr<const PreconditionListProvider>> providers_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_PRECONDITION_LIST_PROVIDER_H_
