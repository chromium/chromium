// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/context_highlight/context_highlight_window_feature.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/context_highlight/context_highlight_overlay_view.h"
#include "chrome/browser/ui/context_highlight/context_highlight_tab_feature.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "components/tabs/public/tab_interface.h"

ContextHighlightWindowFeature::ContextHighlightWindowFeature(
    BrowserWindowInterface& browser)
    : browser_(&browser),
      scoped_data_(browser.GetUnownedUserDataHost(), *this) {
  active_tab_subscription_ = browser_->RegisterActiveTabDidChange(
      base::BindRepeating(&ContextHighlightWindowFeature::OnActiveTabDidChange,
                          base::Unretained(this)));
}

ContextHighlightWindowFeature::~ContextHighlightWindowFeature() = default;

ContextHighlightWindowFeature* ContextHighlightWindowFeature::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

void ContextHighlightWindowFeature::CheckAndUpdateTrackedElementBounds() {
  auto* tab_feature = GetActiveTabFeature();
  OnTrackedElementBoundsChanged(tab_feature->latest_bounds(),
                                tab_feature->latest_scale_factor());
}

void ContextHighlightWindowFeature::OnTrackedElementBoundsChanged(
    const cc::TrackedElementBounds& bounds,
    float device_scale_factor) {
  // View is created lazyly
  if (!context_highlight_view_tracker_.view() && !bounds.empty()) {
    CreateViewForOverlay();
  }

  if (context_highlight_view_tracker_.view()) {
    auto* host_view = BrowserElementsViews::From(browser_)->GetView(
        kContextHighlightViewElementId);

    if (bounds.empty()) {
      host_view->SetVisible(false);
    } else {
      static_cast<ContextHighlightOverlayView*>(
          context_highlight_view_tracker_.view())
          ->UpdateHighlightBounds(bounds, device_scale_factor);
      host_view->SetVisible(true);
    }
  }
}

void ContextHighlightWindowFeature::CreateViewForOverlay() {
  // Grab the host view for the overlay which is owned by the browser view.
  auto* host_view = BrowserElementsViews::From(browser_)->GetView(
      kContextHighlightViewElementId);
  CHECK(host_view);

  // Add the AI overlay view to the host view.
  context_highlight_view_tracker_.SetView(
      host_view->AddChildView(std::make_unique<ContextHighlightOverlayView>()));
}

void ContextHighlightWindowFeature::OnActiveTabDidChange(
    BrowserWindowInterface* browser) {
  CheckAndUpdateTrackedElementBounds();
}

tabs::ContextHighlightTabFeature*
ContextHighlightWindowFeature::GetActiveTabFeature() {
  tabs::TabInterface* tab = browser_->GetActiveTabInterface();
  return tabs::ContextHighlightTabFeature::From(tab);
}

DEFINE_USER_DATA(ContextHighlightWindowFeature);

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ContextHighlightWindowFeature,
                                      kContextHighlightViewElementId);
