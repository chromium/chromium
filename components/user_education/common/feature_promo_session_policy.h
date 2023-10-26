// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SESSION_POLICY_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SESSION_POLICY_H_

#include "base/memory/raw_ptr.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/feature_promo_session_manager.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/feature_promo_storage_service.h"

namespace user_education {

// Describes how IPH interact with each other inside the same session.
class FeaturePromoSessionPolicy {
 public:
  FeaturePromoSessionPolicy();
  virtual ~FeaturePromoSessionPolicy();
  FeaturePromoSessionPolicy(const FeaturePromoSessionPolicy&) = delete;
  void operator=(const FeaturePromoSessionPolicy&) = delete;

  // Determines whether a promo can be displayed based on the current session
  // state. Returns FeaturePromoResult::Success() if the promo is not blocked
  // by session policy, or a specific failure type if it is blocked.
  virtual FeaturePromoResult CanShowPromo(
      const FeaturePromoSpecification& promo_specification) const = 0;
  
  // Notifies the policy that a promo with `promo_specification` has shown.
  // Implementations should update their internal state if this would affect
  // subsequent IPH.
  virtual void OnPromoShown(const FeaturePromoSpecification& promo_specification) = 0;

  void set_session_manager(const FeaturePromoSessionManager* session_manager) {
    session_manager_ = session_manager;
  }
  const FeaturePromoSessionManager* session_manager() const { return session_manager_; }

 private:
  raw_ptr<const FeaturePromoSessionManager> session_manager_ = nullptr;
};

// Implements the legacy session policy, which is to never block a promo due to
// session state. 
class FeaturePromoSessionPolicyV1 : public FeaturePromoSessionPolicy {
 public:
  FeaturePromoSessionPolicyV1();
  ~FeaturePromoSessionPolicyV1() override;

  // FeaturePromoSessionPolicy:
  FeaturePromoResult CanShowPromo(
      const FeaturePromoSpecification& promo_specification) const override;
  void OnPromoShown(const FeaturePromoSpecification& promo_specification) override;
};

// Implements the User Education Experience v2.0 session policy, which is to
// block heavyweight, low-priority promos during the session start grace period
// as well as during a several-day cooldown after another heavyweight promo.
class FeaturePromoSessionPolicyV2 : public FeaturePromoSessionPolicy {
 public:
  FeaturePromoSessionPolicyV2();
  ~FeaturePromoSessionPolicyV2() override;

  // FeaturePromoSessionPolicy:
  FeaturePromoResult CanShowPromo(
      const FeaturePromoSpecification& promo_specification) const override;
  void OnPromoShown(const FeaturePromoSpecification& promo_specification) override;

  void set_storage_service(FeaturePromoStorageService* storage_service) {
    storage_service_ = storage_service;
  }
  FeaturePromoStorageService* storage_service() { return storage_service_; }

 protected:
  FeaturePromoSessionPolicyV2(base::)

 private:
  raw_ptr<FeaturePromoStorageService> storage_service_ = nullptr;
};


}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SESSION_POLICY_H_
