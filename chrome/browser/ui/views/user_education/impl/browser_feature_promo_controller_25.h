// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_25_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_25_H_

#include <map>

#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_controller_25.h"
#include "components/user_education/common/feature_promo/impl/precondition_list_provider.h"
#include "ui/base/interaction/element_identifier.h"

namespace feature_engagement {
class Tracker;
}

namespace ui {
class AcceleratorProvider;
class TrackedElement;
}  // namespace ui

namespace user_education {
class FeaturePromoRegistry;
class FeaturePromoSessionPolicy;
class UserEducationStorageService;
class HelpBubbleFactoryRegistry;
class ProductMessagingController;
class TutorialService;
}  // namespace user_education

class BrowserView;

// Browser implementation of FeaturePromoController for User Education 2.5.
// There is one instance per browser window.
//
// This is implemented in c/b/ui/views specifically because some of the logic
// requires understanding of the existence of views, not because this is a
// views-specific implementation.
class BrowserFeaturePromoController25
    : public user_education::FeaturePromoController25 {
 public:
  // Create the instance for the given |browser_view|. Prefer to call
  // `MaybeCreateForBrowserView()` instead.
  BrowserFeaturePromoController25(
      BrowserView* browser_view,
      feature_engagement::Tracker* feature_engagement_tracker,
      user_education::FeaturePromoRegistry* registry,
      user_education::HelpBubbleFactoryRegistry* help_bubble_registry,
      user_education::UserEducationStorageService* storage_service,
      user_education::FeaturePromoSessionPolicy* session_policy,
      user_education::TutorialService* tutorial_service,
      user_education::ProductMessagingController* messaging_controller);
  ~BrowserFeaturePromoController25() override;

  // FeaturePromoController25:
  void Init() override;

 protected:
  // FeaturePromoController:
  ui::ElementContext GetAnchorContext() const override;
  const ui::AcceleratorProvider* GetAcceleratorProvider() const override;
  std::u16string GetTutorialScreenReaderHint() const override;
  std::u16string GetFocusHelpBubbleScreenReaderHint(
      user_education::FeaturePromoSpecification::PromoType promo_type,
      ui::TrackedElement* anchor_element) const override;
  std::u16string GetBodyIconAltText() const override;
  const base::Feature* GetScreenReaderPromptPromoFeature() const override;
  const char* GetScreenReaderPromptPromoEventName() const override;

  // FeaturePromoController25:
  void AddDemoPreconditionProviders(
      user_education::ComposingPreconditionListProvider& to_add_to,
      bool required) override;
  void AddPreconditionProviders(
      user_education::ComposingPreconditionListProvider& to_add_to,
      Priority priority,
      bool required) override;

 private:
  using PreconditionPtr =
      std::unique_ptr<user_education::FeaturePromoPrecondition>;
  using PreconditionId = user_education::FeaturePromoPrecondition::Identifier;

  // Returns `ptr` as either an object of this type or null, if it is no longer
  // valid.
  static const BrowserFeaturePromoController25* GetFromWeakPtr(
      base::WeakPtr<const user_education::FeaturePromoController> ptr);

  // Gets a forwarding precondition that wraps the precondition with
  // identifier `id` from the `shared_preconditions_` lookup.
  PreconditionPtr WrapSharedPrecondition(PreconditionId id) const;

  // The browser window this instance is responsible for.
  const raw_ptr<BrowserView> browser_view_;

  // Lookup of preconditions that are shared between all or a specific class of
  // promos.
  std::map<PreconditionId, PreconditionPtr> shared_preconditions_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_25_H_
