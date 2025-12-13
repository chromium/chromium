// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller.h"

#include "base/types/pass_key.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/user_education/common/user_education_context.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "components/user_education/webui/tracked_element_help_bubble_webui_anchor.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"

// static
user_education::UserEducationContextPtr
BrowserFeaturePromoControllerBase::GetContextForHelpBubbleImpl(
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
          base::PassKey<BrowserFeaturePromoControllerBase>());
    }
  }
  return nullptr;
}

// static
BrowserWindowInterface* BrowserFeaturePromoControllerBase::GetBrowserForView(
    const views::View* view) {
  if (!view || !view->GetWidget()) {
    return nullptr;
  }

  auto* const browser_view = BrowserView::GetBrowserViewForNativeWindow(
      view->GetWidget()->GetPrimaryWindowWidget()->GetNativeWindow());

  return browser_view ? browser_view->browser() : nullptr;
}
