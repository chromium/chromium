// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_FEATURE_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_FEATURE_PROMO_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/user_education/impl/non_browser_feature_promo_controller.h"

class OmniboxEverywhereService;
class UserEducationService;

namespace feature_engagement {
class Tracker;
}

namespace omnibox_everywhere {

// Omnibox Everywhere implementation of `NonBrowserFeaturePromoController`.
// Managed by `OmniboxEverywhereService`.
// The class allows the management of IPH that are displayed in Omnibox
// Everywhere.
class OmniboxEverywhereFeaturePromoController
    : public NonBrowserFeaturePromoController {
 public:
  OmniboxEverywhereFeaturePromoController(
      feature_engagement::Tracker* tracker_service,
      UserEducationService* user_education_service,
      base::WeakPtr<OmniboxEverywhereService> service);
  ~OmniboxEverywhereFeaturePromoController() override;

 private:
  // NonBrowserFeaturePromoController:
  void AddPreconditionProviders(
      user_education::ComposingPreconditionListProvider& to_add_to,
      Priority priority,
      bool required) override;

  base::WeakPtr<OmniboxEverywhereService> service_;
  base::WeakPtrFactory<OmniboxEverywhereFeaturePromoController> weak_factory_{
      this};
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_FEATURE_PROMO_CONTROLLER_H_
