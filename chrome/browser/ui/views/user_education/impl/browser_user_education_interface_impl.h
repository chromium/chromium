// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_USER_EDUCATION_INTERFACE_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_USER_EDUCATION_INTERFACE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/user_education_context.h"

class BrowserView;
class BrowserWindowInterface;
class Profile;
class UserEducationService;

namespace user_education {
class FeaturePromoController;
}

// Implementation of user education browser feature.
class BrowserUserEducationInterfaceImpl : public BrowserUserEducationInterface {
 public:
  explicit BrowserUserEducationInterfaceImpl(BrowserWindowInterface* browser);
  ~BrowserUserEducationInterfaceImpl() override;

  void Init(BrowserView* browser_view) override;
  void TearDown() override;

  // BrowserUserEducationInterface:
  bool IsFeaturePromoQueued(const base::Feature& iph_feature) const override;
  bool IsFeaturePromoActive(const base::Feature& iph_feature) const override;
  user_education::FeaturePromoResult CanShowFeaturePromo(
      const base::Feature& iph_feature) const override;
  void MaybeShowFeaturePromo(
      user_education::FeaturePromoParams params) override;
  void MaybeShowStartupFeaturePromo(
      user_education::FeaturePromoParams params) override;
  bool AbortFeaturePromo(const base::Feature& iph_feature) override;
  user_education::FeaturePromoHandle CloseFeaturePromoAndContinue(
      const base::Feature& iph_feature) override;
  bool NotifyFeaturePromoFeatureUsed(
      const base::Feature& feature,
      FeaturePromoFeatureUsedAction action) override;
  void NotifyAdditionalConditionEvent(const char* event_name) override;
  user_education::DisplayNewBadge MaybeShowNewBadgeFor(
      const base::Feature& feature) override;
  void NotifyNewBadgeFeatureUsed(const base::Feature& feature) override;

 private:
  // BrowserUserEducationInterface private methods:
  const user_education::UserEducationContextPtr& GetUserEducationContextImpl()
      const override;

  // Gets the corresponding user education service.
  UserEducationService* GetUserEducationService();
  const UserEducationService* GetUserEducationService() const;

  // Gets the corresponding FeaturePromoController.
  user_education::FeaturePromoController* GetFeaturePromoController();
  const user_education::FeaturePromoController* GetFeaturePromoController()
      const;

  // Called on the frame after the browser window is constructed; processes
  // pending startup promos.
  void CompleteInitialization();

  void ClearQueuedPromos(
      user_education::FeaturePromoResult::Failure failure =
          user_education::FeaturePromoResult::Failure::kError);

  // Implementation for showing a startup promo.
  void MaybeShowStartupFeaturePromoImpl(
      user_education::FeaturePromoParams params);

  enum class State {
    kUninitialized,
    kInitializationPending,
    kInitialized,
    kTornDown
  };

  State state_ = State::kUninitialized;
  raw_ptr<Profile> profile_ = nullptr;
  std::vector<user_education::FeaturePromoParams> queued_params_;
  user_education::UserEducationContextPtr user_education_context_;
  base::WeakPtrFactory<BrowserUserEducationInterfaceImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_USER_EDUCATION_INTERFACE_IMPL_H_
