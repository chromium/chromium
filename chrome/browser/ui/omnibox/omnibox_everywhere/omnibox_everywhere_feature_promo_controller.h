// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_FEATURE_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_FEATURE_PROMO_CONTROLLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_controller_impl.h"
#include "components/user_education/common/user_education_context.h"

class OmniboxEverywhereService;
class UserEducationService;

namespace omnibox_everywhere {

// Omnibox Everywhere implementation of `FeaturePromoControllerImpl`.
// Managed by `OmniboxEverywhereService`.
// The class allows the management of IPH that are displayed in Omnibox
// Everywhere.
class OmniboxEverywhereFeaturePromoController
    : public user_education::FeaturePromoControllerImpl {
 public:
  OmniboxEverywhereFeaturePromoController(
      feature_engagement::Tracker* tracker_service,
      UserEducationService* user_education_service,
      base::WeakPtr<OmniboxEverywhereService> service);
  ~OmniboxEverywhereFeaturePromoController() override;

  using user_education::FeaturePromoControllerImpl::MaybeShowPromo;
  void MaybeShowPromo(user_education::FeaturePromoParams params);

 private:
  // FeaturePromoControllerImpl:
  void AddPreconditionProviders(
      user_education::ComposingPreconditionListProvider& to_add_to,
      Priority priority,
      bool required) override;

  // user_education::FeaturePromoControllerImpl:
  std::u16string GetBodyIconAltText() const override;
  const base::Feature* GetScreenReaderPromptPromoFeature() const override;
  const char* GetScreenReaderPromptPromoEventName() const override;
  std::u16string GetTutorialScreenReaderHint(
      const ui::AcceleratorProvider* accelerator_provider) const override;
  std::u16string GetFocusHelpBubbleScreenReaderHint(
      user_education::FeaturePromoSpecification::PromoType promo_type,
      ui::TrackedElement* anchor_element,
      const ui::AcceleratorProvider* accelerator_provider) const override;
  user_education::UserEducationContextPtr GetContextForHelpBubble(
      const ui::TrackedElement* anchor_element) const override;

  base::WeakPtr<OmniboxEverywhereService> service_;
  user_education::UserEducationContextPtr context_;
  base::WeakPtrFactory<OmniboxEverywhereFeaturePromoController> weak_factory_{
      this};
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_FEATURE_PROMO_CONTROLLER_H_
