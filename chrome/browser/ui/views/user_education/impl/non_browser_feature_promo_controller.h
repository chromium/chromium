// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_NON_BROWSER_FEATURE_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_NON_BROWSER_FEATURE_PROMO_CONTROLLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_controller_impl.h"
#include "components/user_education/common/user_education_context.h"

namespace feature_engagement {
class Tracker;
}

namespace omnibox_everywhere {
class OmniboxEverywhereFeaturePromoController;
}  // namespace omnibox_everywhere

namespace ui {
class AcceleratorProvider;
class TrackedElement;
}  // namespace ui

class ProfilePickerFeaturePromoController;
class UserEducationService;

// Base `FeaturePromoControllerImpl` for User Education experiences that operate
// independently of standard browser windows.
//
// Most browser UI surfaces should reuse the existing per-profile
// `FeaturePromoController`, optionally providing a customized
// `UserEducationContext`. Subclassing this controller is strictly intended for
// special-purpose surfaces where that model is not viable:
//  - Profiles that do not back a regular browser window session (e.g. the
//    off-the-record profile backing the Profile Picker).
//  - Isolated UI entrypoints requiring fundamentally distinct precondition
//    chains or activation lifecycles (e.g. Omnibox Everywhere).
class NonBrowserFeaturePromoController
    : public user_education::FeaturePromoControllerImpl {
 public:
  using PassKey = base::PassKey<
      ProfilePickerFeaturePromoController,
      omnibox_everywhere::OmniboxEverywhereFeaturePromoController>;

  ~NonBrowserFeaturePromoController() override;

  using user_education::FeaturePromoControllerImpl::MaybeShowPromo;
  void MaybeShowPromo(user_education::FeaturePromoParams params);

  void set_context(user_education::UserEducationContextPtr context) {
    context_ = std::move(context);
  }
  user_education::UserEducationContextPtr context() const { return context_; }

  void set_accelerator_provider(
      const ui::AcceleratorProvider* accelerator_provider) {
    accelerator_provider_ = accelerator_provider;
  }
  const ui::AcceleratorProvider* accelerator_provider() const {
    return accelerator_provider_;
  }

 protected:
  NonBrowserFeaturePromoController(
      PassKey pass_key,
      feature_engagement::Tracker* tracker_service,
      UserEducationService* user_education_service,
      user_education::UserEducationContextPtr context = nullptr,
      const ui::AcceleratorProvider* accelerator_provider = nullptr);

  // FeaturePromoControllerImpl:
  void AddPreconditionProviders(
      user_education::ComposingPreconditionListProvider& to_add_to,
      Priority priority,
      bool required) override;
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

 private:
  user_education::UserEducationContextPtr context_;
  raw_ptr<const ui::AcceleratorProvider> accelerator_provider_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_NON_BROWSER_FEATURE_PROMO_CONTROLLER_H_
