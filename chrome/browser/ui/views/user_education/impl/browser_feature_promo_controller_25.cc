// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller_25.h"

#include <string>

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_preconditions.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/interaction/element_tracker_views.h"

BrowserFeaturePromoController25::BrowserFeaturePromoController25(
    BrowserView* browser_view,
    feature_engagement::Tracker* feature_engagement_tracker,
    user_education::FeaturePromoRegistry* registry,
    user_education::HelpBubbleFactoryRegistry* help_bubble_registry,
    user_education::UserEducationStorageService* storage_service,
    user_education::FeaturePromoSessionPolicy* session_policy,
    user_education::TutorialService* tutorial_service,
    user_education::ProductMessagingController* messaging_controller)
    : FeaturePromoController25(feature_engagement_tracker,
                               registry,
                               help_bubble_registry,
                               storage_service,
                               session_policy,
                               tutorial_service,
                               messaging_controller),
      browser_view_(browser_view) {}

BrowserFeaturePromoController25::~BrowserFeaturePromoController25() {
  OnDestroying();
}

ui::ElementContext BrowserFeaturePromoController25::GetAnchorContext() const {
  return views::ElementTrackerViews::GetContextForView(browser_view_);
}

const ui::AcceleratorProvider*
BrowserFeaturePromoController25::GetAcceleratorProvider() const {
  return browser_view_;
}

std::u16string BrowserFeaturePromoController25::GetTutorialScreenReaderHint()
    const {
  return BrowserHelpBubble::GetFocusTutorialBubbleScreenReaderHint(
      browser_view_);
}

std::u16string
BrowserFeaturePromoController25::GetFocusHelpBubbleScreenReaderHint(
    user_education::FeaturePromoSpecification::PromoType promo_type,
    ui::TrackedElement* anchor_element) const {
  return BrowserHelpBubble::GetFocusHelpBubbleScreenReaderHint(
      promo_type, browser_view_, anchor_element);
}

std::u16string BrowserFeaturePromoController25::GetBodyIconAltText() const {
  return l10n_util::GetStringUTF16(IDS_CHROME_TIP);
}

const base::Feature*
BrowserFeaturePromoController25::GetScreenReaderPromptPromoFeature() const {
  return &feature_engagement::kIPHFocusHelpBubbleScreenReaderPromoFeature;
}

const char*
BrowserFeaturePromoController25::GetScreenReaderPromptPromoEventName() const {
  return feature_engagement::events::kFocusHelpBubbleAcceleratorPromoRead;
}

void BrowserFeaturePromoController25::Init() {
  FeaturePromoController25::Init();

  // Create shared preconditions. This should be called after the browser view
  // is set.
  CHECK(browser_view_);
  CHECK(shared_preconditions_.empty());
  PreconditionPtr ptr =
      std::make_unique<OmniboxNotOpenPrecondition>(*browser_view_);
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);
  ptr = std::make_unique<ContentNotFullscreenPrecondition>(
      *browser_view_->browser());
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);
  ptr = std::make_unique<ToolbarNotCollapsedPrecondition>(*browser_view_);
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);
  ptr = std::make_unique<BrowserNotClosingPrecondition>(*browser_view_);
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);
  ptr = std::make_unique<NoCriticalNoticeShowingPrecondition>(*browser_view_);
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);
  // Ensure that this uses the same time source as the rest of the User
  // Education system, so tests are consistent.
  ptr = std::make_unique<UserNotActivePrecondition>(*browser_view_,
                                                    *storage_service());
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);
}

void BrowserFeaturePromoController25::AddDemoPreconditionProviders(
    user_education::ComposingPreconditionListProvider& to_add_to,
    bool required) {
  FeaturePromoController25::AddDemoPreconditionProviders(to_add_to, required);
  if (required) {
    to_add_to.AddProvider(base::BindRepeating(
        [](base::WeakPtr<const user_education::FeaturePromoController> ptr,
           const user_education::FeaturePromoSpecification&,
           const user_education::FeaturePromoParams&) {
          user_education::FeaturePromoPreconditionList preconditions;
          if (const auto* const controller = GetFromWeakPtr(ptr)) {
            preconditions.AddPrecondition(controller->WrapSharedPrecondition(
                kBrowserNotClosingPrecondition));
          }
          return preconditions;
        },
        GetAsWeakPtr()));
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
        [](base::WeakPtr<const user_education::FeaturePromoController> ptr,
           const user_education::FeaturePromoSpecification&,
           const user_education::FeaturePromoParams&) {
          user_education::FeaturePromoPreconditionList preconditions;
          if (const auto* const controller = GetFromWeakPtr(ptr)) {
            preconditions.AddPrecondition(controller->WrapSharedPrecondition(
                kBrowserNotClosingPrecondition));
            preconditions.AddPrecondition(controller->WrapSharedPrecondition(
                kNoCriticalNoticeShowingPrecondition));
          }
          return preconditions;
        },
        GetAsWeakPtr()));
  } else {
    to_add_to.AddProvider(base::BindRepeating(
        [](base::WeakPtr<const user_education::FeaturePromoController> ptr,
           const user_education::FeaturePromoSpecification& spec,
           const user_education::FeaturePromoParams&) {
          user_education::FeaturePromoPreconditionList preconditions;
          if (const auto* const controller = GetFromWeakPtr(ptr)) {
            // Check that the window isn't active.
            if (!spec.is_exempt_from(kWindowActivePrecondition)) {
              preconditions.AddPrecondition(
                  std::make_unique<WindowActivePrecondition>());
            }

            // The rest of the preconditions are shared. This helper adds a
            // shared condition if it's not excluded by the promo specification.
            auto maybe_add_shared_precondition =
                [&spec, &preconditions, &controller](
                    user_education::FeaturePromoPrecondition::Identifier id) {
                  if (!spec.is_exempt_from(id)) {
                    preconditions.AddPrecondition(
                        controller->WrapSharedPrecondition(id));
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
            const auto info =
                controller->session_policy()->GetPromoPriorityInfo(spec);
            if ((info.priority == Priority::kLow &&
                 info.weight == PromoWeight::kHeavy) ||
                spec.focus_on_show_override() == false) {
              // Since heavyweight promos steal keyboard focus, try not to show
              // them when the user is typing.
              maybe_add_shared_precondition(kUserNotActivePrecondition);
            }
          }
          return preconditions;
        },
        GetAsWeakPtr()));
  }
}

// static
const BrowserFeaturePromoController25*
BrowserFeaturePromoController25::GetFromWeakPtr(
    base::WeakPtr<const user_education::FeaturePromoController> ptr) {
  const auto* const actual = ptr.get();
  return actual ? static_cast<const BrowserFeaturePromoController25*>(actual)
                : nullptr;
}

BrowserFeaturePromoController25::PreconditionPtr
BrowserFeaturePromoController25::WrapSharedPrecondition(
    PreconditionId id) const {
  const auto it = shared_preconditions_.find(id);
  CHECK(it != shared_preconditions_.end());
  return std::make_unique<user_education::ForwardingFeaturePromoPrecondition>(
      *it->second);
}
