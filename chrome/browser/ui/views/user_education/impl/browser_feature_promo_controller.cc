// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller.h"

#include "base/types/pass_key.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_preconditions.h"
#include "chrome/browser/ui/views/user_education/impl/browser_user_education_context.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/user_education_context.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "components/user_education/webui/tracked_element_help_bubble_webui_anchor.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/interaction/element_tracker_views.h"

BrowserFeaturePromoController::~BrowserFeaturePromoController() {
  OnDestroying();
}

void BrowserFeaturePromoController::AddDemoPreconditionProviders(
    user_education::ComposingPreconditionListProvider& to_add_to,
    bool required) {
  FeaturePromoControllerImpl::AddDemoPreconditionProviders(to_add_to, required);
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

void BrowserFeaturePromoController::AddPreconditionProviders(
    user_education::ComposingPreconditionListProvider& to_add_to,
    Priority priority,
    bool required) {
  FeaturePromoControllerImpl::AddPreconditionProviders(to_add_to, priority,
                                                       required);

  if (required) {
    to_add_to.AddProvider(base::BindRepeating(
        [](const user_education::FeaturePromoSessionPolicy* policy,
           const user_education::FeaturePromoSpecification& spec,
           const user_education::FeaturePromoParams&,
           const user_education::UserEducationContextPtr& context) {
          auto* browser_context = context->AsA<BrowserUserEducationContext>();
          CHECK(browser_context);
          user_education::FeaturePromoPreconditionList preconditions;
          preconditions.AddPrecondition(browser_context->GetSharedPrecondition(
              kBrowserNotClosingPrecondition));
          preconditions.AddPrecondition(browser_context->GetSharedPrecondition(
              kNoCriticalNoticeShowingPrecondition));

          const auto info = policy->GetPromoPriorityInfo(spec);
          if (info.priority != Priority::kHigh) {
            preconditions.AddPrecondition(
                browser_context->GetSharedPrecondition(
                    kActorNotActuatingActiveTabPrecondition));
          }
          if (info.priority == Priority::kLow &&
              spec.promo_type() != user_education::FeaturePromoSpecification::
                                       PromoType::kToast) {
            preconditions.AddPrecondition(
                browser_context->GetSharedPrecondition(
                    kEnterprisePolicyNotBlockingPrecondition));
          }
          return preconditions;
        },
        base::Unretained(session_policy())));
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

std::u16string BrowserFeaturePromoController::GetTutorialScreenReaderHint(
    const ui::AcceleratorProvider* accelerator_provider) const {
  return BrowserHelpBubble::GetFocusTutorialBubbleScreenReaderHint(
      accelerator_provider);
}

std::u16string
BrowserFeaturePromoController::GetFocusHelpBubbleScreenReaderHint(
    user_education::FeaturePromoSpecification::PromoType promo_type,
    ui::TrackedElement* anchor_element,
    const ui::AcceleratorProvider* accelerator_provider) const {
  return BrowserHelpBubble::GetFocusHelpBubbleScreenReaderHint(
      promo_type, accelerator_provider, anchor_element);
}

std::u16string BrowserFeaturePromoController::GetBodyIconAltText() const {
  return l10n_util::GetStringUTF16(IDS_CHROME_TIP);
}

const base::Feature*
BrowserFeaturePromoController::GetScreenReaderPromptPromoFeature() const {
  return &feature_engagement::kIPHFocusHelpBubbleScreenReaderPromoFeature;
}

const char* BrowserFeaturePromoController::GetScreenReaderPromptPromoEventName()
    const {
  return feature_engagement::events::kFocusHelpBubbleAcceleratorPromoRead;
}

user_education::UserEducationContextPtr
BrowserFeaturePromoController::GetContextForHelpBubble(
    const ui::TrackedElement* anchor_element) const {
  return GetContextForHelpBubbleImpl(anchor_element);
}

// static
BrowserWindowInterface* BrowserFeaturePromoController::GetBrowserForView(
    const views::View* view) {
  if (!view || !view->GetWidget()) {
    return nullptr;
  }

  auto* const browser_view = BrowserView::GetBrowserViewForNativeWindow(
      view->GetWidget()->GetPrimaryWindowWidget()->GetNativeWindow());

  return browser_view ? browser_view->browser() : nullptr;
}

// static
user_education::UserEducationContextPtr
BrowserFeaturePromoController::GetContextForHelpBubbleImpl(
    const ui::TrackedElement* anchor_element) {
  if (!anchor_element) {
    return nullptr;
  }
  BrowserWindowInterface* browser = nullptr;
  if (auto* const view_element =
          anchor_element->AsA<views::TrackedElementViews>()) {
    browser = GetBrowserForView(view_element->view());
  } else if (auto* const webui_element =
                 anchor_element->AsA<
                     user_education::TrackedElementHelpBubbleWebUIAnchor>()) {
    browser = webui::GetBrowserWindowInterface(
        webui_element->handler()->GetWebContents());
  }
  if (browser) {
    if (auto* interface = BrowserUserEducationInterface::From(browser)) {
      return interface->GetUserEducationContext(
          base::PassKey<BrowserFeaturePromoController>());
    }
  }
  return nullptr;
}
