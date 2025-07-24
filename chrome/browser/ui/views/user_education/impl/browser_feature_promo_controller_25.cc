// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller_25.h"

#include <string>

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_preconditions.h"
#include "chrome/browser/ui/views/user_education/impl/browser_user_education_context.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/user_education_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/interaction/element_tracker_views.h"

BrowserFeaturePromoController25::~BrowserFeaturePromoController25() {
  OnDestroying();
}

void BrowserFeaturePromoController25::AddDemoPreconditionProviders(
    user_education::ComposingPreconditionListProvider& to_add_to,
    bool required) {
  FeaturePromoController25::AddDemoPreconditionProviders(to_add_to, required);
  if (required) {
    to_add_to.AddProvider(base::BindRepeating(
        [](const user_education::FeaturePromoSpecification&,
           const user_education::FeaturePromoParams&,
           const user_education::UserEducationContextPtr& context) {
          auto* browser_context = context->AsA<BrowserUserEducationContext>();
          CHECK(browser_context);
          user_education::FeaturePromoPreconditionList preconditions;
          preconditions.AddPrecondition(browser_context->GetSharedPrecondition(
              kBrowserNotClosingPrecondition));
          return preconditions;
        }));
  }
}

void BrowserFeaturePromoController25::AddPreconditionProviders(
    user_education::ComposingPreconditionListProvider& to_add_to,
    Priority priority,
    bool required) {
  FeaturePromoController25::AddPreconditionProviders(to_add_to, priority,
                                                     required);

  if (required) {
    to_add_to.AddProvider(base::BindRepeating(
        [](const user_education::FeaturePromoSpecification&,
           const user_education::FeaturePromoParams&,
           const user_education::UserEducationContextPtr& context) {
          auto* browser_context = context->AsA<BrowserUserEducationContext>();
          CHECK(browser_context);
          user_education::FeaturePromoPreconditionList preconditions;
          preconditions.AddPrecondition(browser_context->GetSharedPrecondition(
              kBrowserNotClosingPrecondition));
          preconditions.AddPrecondition(browser_context->GetSharedPrecondition(
              kNoCriticalNoticeShowingPrecondition));
          return preconditions;
        }));
  } else {
    to_add_to.AddProvider(base::BindRepeating(
        [](const user_education::FeaturePromoSessionPolicy* policy,
           const user_education::FeaturePromoSpecification& spec,
           const user_education::FeaturePromoParams&,
           const user_education::UserEducationContextPtr& context) {
          auto* browser_context = context->AsA<BrowserUserEducationContext>();
          CHECK(browser_context);
          user_education::FeaturePromoPreconditionList preconditions;
          if (!spec.is_exempt_from(kWindowActivePrecondition)) {
            preconditions.AddPrecondition(
                std::make_unique<WindowActivePrecondition>());
          }

          // The rest of the preconditions are shared. This helper adds a
          // shared condition if it's not excluded by the promo specification.
          auto maybe_add_shared_precondition =
              [&spec, &preconditions, &browser_context](
                  user_education::FeaturePromoPrecondition::Identifier id) {
                if (!spec.is_exempt_from(id)) {
                  preconditions.AddPrecondition(
                      browser_context->GetSharedPrecondition(id));
                }
              };

          // Promos shouldn't show when content is fullscreen.
          maybe_add_shared_precondition(kContentNotFullscreenPrecondition);

          // Most promos are blocked by an open omnibox to prevent z-fighting
          // issues.
          maybe_add_shared_precondition(kOmniboxNotOpenPrecondition);

          // Most promos do not show when the toolbar is collapsed because (a)
          // the anchor might not be in the expected place - or visible at
          // all - and (b) the browser window might not be large enough to
          // properly accommodate the promo bubble, or use the feature being
          // promoted.
          maybe_add_shared_precondition(kToolbarNotCollapsedPrecondition);

          // Higher priority and lightweight messages are not subject to
          // certain requirements.
          const auto info = policy->GetPromoPriorityInfo(spec);
          if ((info.priority == Priority::kLow &&
               info.weight == PromoWeight::kHeavy) ||
              spec.focus_on_show_override() == false) {
            // Since heavyweight promos steal keyboard focus, try not to show
            // them when the user is typing.
            maybe_add_shared_precondition(kUserNotActivePrecondition);
          }
          return preconditions;
        },
        base::Unretained(session_policy())));
  }
}
