// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/autofill_help_bubble_factory.h"

#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble_event_relay.h"
#include "components/user_education/views/help_bubble_view.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget_delegate.h"

DEFINE_FRAMEWORK_SPECIFIC_METADATA(AutofillHelpBubbleFactory)

namespace {

// Returns the browser widget if `el` is an autofill popup, null otherwise.
views::Widget* GetBrowserWidgetFrom(const ui::TrackedElement* el) {
  if (auto* view_el = el->AsA<views::TrackedElementViews>()) {
    auto* const view = view_el->view();
    if (!view->GetWidget() || !view->GetWidget()->widget_delegate()) {
      return nullptr;
    }
    if (auto* contents =
            view->GetWidget()->widget_delegate()->GetContentsView()) {
      if (auto* popup = views::AsViewClass<autofill::PopupBaseView>(contents)) {
        if (auto* browser =
                BrowserView::GetBrowserViewForBrowser(popup->GetBrowser())) {
          return browser->GetWidget();
        }
      }
    }
  }
  return nullptr;
}

}  // namespace

AutofillHelpBubbleFactory::AutofillHelpBubbleFactory(
    const user_education::HelpBubbleDelegate* delegate)
    : HelpBubbleFactoryViews(delegate) {}

AutofillHelpBubbleFactory::~AutofillHelpBubbleFactory() = default;

bool AutofillHelpBubbleFactory::CanBuildBubbleForTrackedElement(
    const ui::TrackedElement* element) const {
  return GetBrowserWidgetFrom(element) != nullptr;
}

std::unique_ptr<user_education::HelpBubble>
AutofillHelpBubbleFactory::CreateBubble(
    ui::TrackedElement* element,
    user_education::HelpBubbleParams params) {
  user_education::internal::HelpBubbleAnchorParams anchor;
  anchor.view = element->AsA<views::TrackedElementViews>()->view();
  return CreateBubbleImpl(
      element, anchor, std::move(params),
      CreateWindowHelpBubbleEventRelay(GetBrowserWidgetFrom(element)));
}
