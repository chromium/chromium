// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/browser_user_education_context.h"

#include <optional>

#include "base/callback_list.h"
#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_preconditions.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/user_education_context.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/safe_castable.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/view_subregion_anchor.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

// Forwarding precondition that releases its reference when the context is
// invalidated or destroyed.
class BrowserUserEducationContext::ForwardingPrecondition
    : public user_education::ForwardingFeaturePromoPrecondition {
 public:
  ForwardingPrecondition(const FeaturePromoPrecondition& source,
                         BrowserUserEducationContext& context)
      : ForwardingFeaturePromoPrecondition(source),
        invalidate_subscription_(context.invalidate_callbacks_.Add(
            base::BindOnce(&ForwardingPrecondition::Invalidate,
                           base::Unretained(this)))) {}

 private:
  base::CallbackListSubscription invalidate_subscription_;
};

DEFINE_SAFE_CAST_TARGET(BrowserUserEducationContext)

BrowserUserEducationContext::BrowserUserEducationContext(
    BrowserView& browser_view,
    const user_education::UserEducationTimeProvider& time_provider)
    : browser_view_(&browser_view) {
  CreateSharedPreconditions(time_provider);
}

BrowserUserEducationContext::~BrowserUserEducationContext() {
  invalidate_callbacks_.Notify();
}

bool BrowserUserEducationContext::IsValid() const {
  return browser_view_.get();
}

ui::ElementContext BrowserUserEducationContext::GetElementContext() const {
  CHECK(IsValid());
  return browser_view_->GetElementContext();
}

const ui::AcceleratorProvider*
BrowserUserEducationContext::GetAcceleratorProvider() const {
  CHECK(IsValid());
  return browser_view_.get();
}

user_education::AnchorElementFilter
BrowserUserEducationContext::GetDefaultElementFilter() const {
  CHECK(IsValid());
  return base::BindRepeating(&BrowserUserEducationContext::Filter,
                             GetElementContext(),
                             browser_view_->GetProfile()->GetWeakPtr());
}

BrowserUserEducationContext::PreconditionPtr
BrowserUserEducationContext::GetSharedPrecondition(PreconditionId id) {
  const auto it = shared_preconditions_.find(id);
  CHECK(it != shared_preconditions_.end());
  return std::make_unique<ForwardingPrecondition>(*it->second, *this);
}

void BrowserUserEducationContext::Invalidate(
    base::PassKey<BrowserUserEducationInterfaceImpl>) {
  invalidate_callbacks_.Notify();
  shared_preconditions_.clear();
  browser_view_ = nullptr;
}

BrowserWindowInterface* BrowserUserEducationContext::GetBrowser() const {
  CHECK(IsValid());
  return browser_view_->browser();
}

BrowserView& BrowserUserEducationContext::GetBrowserView() const {
  CHECK(IsValid());
  return *browser_view_;
}

void BrowserUserEducationContext::CreateSharedPreconditions(
    const user_education::UserEducationTimeProvider& time_provider) {
  CHECK(shared_preconditions_.empty());

  // Hold off showing most promos while the omnibox is open.
  PreconditionPtr ptr =
      std::make_unique<OmniboxNotOpenPrecondition>(*browser_view_);
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);

  // Hold off most promos when the content is in fullscreen.
  ptr = std::make_unique<ContentNotFullscreenPrecondition>(
      *browser_view_->browser());
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);

  // Hold off most promos when the toolbar is collapsed.
  ptr = std::make_unique<ToolbarNotCollapsedPrecondition>(*browser_view_);
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);

  // Do not show promos when the browser is closing.
  ptr = std::make_unique<BrowserNotClosingPrecondition>(*browser_view_);
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);

  // Do not show promos when a critical notice is showing.
  ptr = std::make_unique<NoCriticalNoticeShowingPrecondition>(*browser_view_);
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);

  // Ensure that this uses the same time source as the rest of the User
  // Education system, so tests are consistent.
  ptr = std::make_unique<UserNotActivePrecondition>(*browser_view_,
                                                    time_provider);
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);

  // Do not show certain promos when in Enterprise no-promos mode.
  ptr = std::make_unique<EnterprisePolicyNotBlockingPrecondition>();
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);

  // Do not show certain promos while the actor is actuating the active tab.
  ptr = std::make_unique<ActorNotActuatingActiveTabPrecondition>(
      *browser_view_->browser());
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);
}

ui::TrackedElement* BrowserUserEducationContext::Filter(
    ui::ElementContext default_context,
    base::WeakPtr<content::BrowserContext> profile,
    const ui::ElementTracker::ElementList& candidates) {
  if (!profile) {
    return nullptr;
  }

  // Find elements in the default context.
  for (auto* const element : candidates) {
    // Always prefer elements in the default context.
    if (element->context() == default_context) {
      return element;
    }

    // Web elements may have a different context, but will set in the same
    // View/Widget hierarchy.
    if (auto* const web_el = element->AsA<ui::TrackedElementWebUI>()) {
      if (auto* const view = web_el->GetWebView()) {
        const auto webview_context =
            views::ElementTrackerViews::GetInstance()->GetContextForView(view);
        if (webview_context == default_context) {
          return element;
        }
      }
    }
  }

  // No elements in the default context. Check for other contexts in active
  // windows in the same profile.
  ui::TrackedElement* background_element = nullptr;
  for (auto* const element : candidates) {
    // Find where this element is in the Views hierarchy.
    const views::View* view = nullptr;
    if (auto* const view_el = element->AsA<views::TrackedElementViews>()) {
      view = view_el->view();
    } else if (auto* const view_region_el =
                   element->AsA<views::ViewSubregionAnchor>()) {
      view = &view_region_el->view();
    } else if (auto* const webui_el = element->AsA<ui::TrackedElementWebUI>()) {
      // Reject WebUI in different profiles. It is not necessary to look up the
      // View if it can be rejected here.
      if (webui_el->handler()->web_contents()->GetBrowserContext() !=
          profile.get()) {
        continue;
      }
      view = webui_el->GetWebView();
    }

    // Find the primary widget, which should be in the foreground.
    if (view && view->GetWidget()) {
      auto* const primary = view->GetWidget()->GetPrimaryWindowWidget();

      // Rule out browsers not in this profile. [Reluctantly] accept elements in
      // Widgets not in any profile.
      if (const auto* const browser_view =
              BrowserView::GetBrowserViewForNativeWindow(
                  primary->GetNativeWindow());
          browser_view && browser_view->GetProfile() != profile.get()) {
        continue;
      }

      // Strongly prefer elements in active windows.
      if (primary->ShouldPaintAsActive()) {
        return element;
      }

      // Worst case, return an element in an inactive window.
      if (!background_element) {
        background_element = element;
      }
    }
  }

  // Fall back to an element in a background window of the current profile, if
  // one was found.
  return background_element;
}
