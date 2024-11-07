// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_PRECONDITION_LIST_PROVIDER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_PRECONDITION_LIST_PROVIDER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"

namespace user_education {

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
      const FeaturePromoSpecification& spec) const = 0;
};

// Factory that returns a combination of preconditions from other providers.
class ComposingPreconditionListProvider : public PreconditionListProvider {
 public:
  ComposingPreconditionListProvider();
  ~ComposingPreconditionListProvider() override;

  void AddProvider(const PreconditionListProvider* provider);

  // PreconditionListProvider:
  FeaturePromoPreconditionList GetPreconditions(
      const FeaturePromoSpecification& spec) const override;

 private:
  std::vector<raw_ptr<const PreconditionListProvider>> providers_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_PRECONDITION_LIST_PROVIDER_H_
