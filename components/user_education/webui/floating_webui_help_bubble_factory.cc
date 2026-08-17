// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/webui/floating_webui_help_bubble_factory.h"

#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/safe_castable.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

namespace user_education {

namespace {

// Attempts to extract the host WebView from `element`; returns null if
// `element` is not a TrackedElementWebUI or the host view
// cannot be determined.
views::WebView* GetWebViewForElement(const ui::TrackedElement* element) {
  if (const auto* const element_webui =
          element->AsA<ui::TrackedElementWebUI>()) {
    return element_webui->GetWebView();
  }
  return nullptr;
}

}  // namespace

FloatingWebUIHelpBubbleFactory::FloatingWebUIHelpBubbleFactory(
    const HelpBubbleDelegate* delegate)
    : HelpBubbleFactoryViews(delegate) {}
FloatingWebUIHelpBubbleFactory::~FloatingWebUIHelpBubbleFactory() = default;

DEFINE_SAFE_CAST_TARGET(FloatingWebUIHelpBubbleFactory)

std::unique_ptr<HelpBubble> FloatingWebUIHelpBubbleFactory::CreateBubble(
    ui::TrackedElement* element,
    HelpBubbleParams params) {
  internal::HelpBubbleAnchorParams anchor;
  anchor.view = GetWebViewForElement(element);
  anchor.rect = element->GetScreenBounds();
  auto result = CreateBubbleImpl(element, anchor, std::move(params), nullptr);
  element->AsA<ui::TrackedElementWebUI>()
      ->handler()
      ->GetHelpBubbleHandler()
      ->OnFloatingHelpBubbleCreated(element->AsA<ui::TrackedElementWebUI>(),
                                    result.get());
  return result;
}

bool FloatingWebUIHelpBubbleFactory::CanBuildBubbleForTrackedElement(
    const ui::TrackedElement* element) const {
  return GetWebViewForElement(element) != nullptr;
}

}  // namespace user_education
