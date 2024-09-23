// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/webui/floating_webui_help_bubble_factory.h"

#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "components/user_education/webui/tracked_element_webui.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace user_education {

namespace {

// Searches `from_view` recursively (depth-first) for a WebView with `contents`.
views::WebView* FindWebViewWithContentsRecursive(
    views::View* from_view,
    const content::WebContents* contents) {
  auto* const web_view = views::AsViewClass<views::WebView>(from_view);
  if (web_view && web_view->web_contents() == contents) {
    return web_view;
  }

  for (views::View* const child_view : from_view->children()) {
    auto* const result = FindWebViewWithContentsRecursive(child_view, contents);
    if (result) {
      return result;
    }
  }

  return nullptr;
}

// Attempts to extract the host WebView from `element`; returns null if
// `element` is not a TrackedElementWebUI or the host view cannot be determined.
views::WebView* GetWebViewForElement(const ui::TrackedElement* element) {
  if (!element->IsA<TrackedElementWebUI>()) {
    return nullptr;
  }
  const auto* const element_webui = element->AsA<TrackedElementWebUI>();
  auto* const contents = element_webui->handler()->GetWebContents();
  if (!contents) {
    return nullptr;
  }
  auto* const widget = views::Widget::GetWidgetForNativeWindow(
      contents->GetTopLevelNativeWindow());
  if (!widget) {
    return nullptr;
  }
  return FindWebViewWithContentsRecursive(widget->GetContentsView(), contents);
}

}  // namespace

FloatingWebUIHelpBubbleFactory::FloatingWebUIHelpBubbleFactory(
    const HelpBubbleDelegate* delegate)
    : HelpBubbleFactoryViews(delegate) {}
FloatingWebUIHelpBubbleFactory::~FloatingWebUIHelpBubbleFactory() = default;

DEFINE_FRAMEWORK_SPECIFIC_METADATA(FloatingWebUIHelpBubbleFactory)

std::unique_ptr<HelpBubble> FloatingWebUIHelpBubbleFactory::CreateBubble(
    ui::TrackedElement* element,
    HelpBubbleParams params) {
  internal::HelpBubbleAnchorParams anchor;
  anchor.view = GetWebViewForElement(element);
  anchor.rect = element->GetScreenBounds();
  auto result = CreateBubbleImpl(element, anchor, std::move(params), nullptr);
  element->AsA<TrackedElementWebUI>()->handler()->OnFloatingHelpBubbleCreated(
      element->identifier(), result.get());
  return result;
}

bool FloatingWebUIHelpBubbleFactory::CanBuildBubbleForTrackedElement(
    const ui::TrackedElement* element) const {
  return GetWebViewForElement(element) != nullptr;
}

}  // namespace user_education
