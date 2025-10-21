// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/interaction/browser_elements_views.h"

#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/views/interaction/element_tracker_views.h"

DEFINE_TYPED_IDENTIFIER_VALUE(views::WebView,
                              kActiveContentsWebViewRetrievalId);

DEFINE_FRAMEWORK_SPECIFIC_METADATA(BrowserElementsViews)

BrowserElementsViews::BrowserElementsViews(BrowserWindowInterface& browser)
    : BrowserElements(browser) {}

BrowserElementsViews::~BrowserElementsViews() = default;

views::Widget* BrowserElementsViews::GetPrimaryWindowWidget() {
  const auto context = GetContext();
  return context
             ? views::ElementTrackerViews::GetInstance()->GetWidgetForContext(
                   context)
             : nullptr;
}

void BrowserElementsViews::TearDown() {
  retrieval_callbacks_.clear();
}

// static
BrowserElementsViews* BrowserElementsViews::From(
    BrowserWindowInterface* browser) {
  auto* const base = BrowserElements::From(browser);
  return base ? base->AsA<BrowserElementsViews>() : nullptr;
}

views::View* BrowserElementsViews::GetView(ui::ElementIdentifier id) {
  return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
      id, GetContext());
}

BrowserElementsViews::ViewList BrowserElementsViews::GetAllViews(
    ui::ElementIdentifier id) {
  return views::ElementTrackerViews::GetInstance()->GetAllMatchingViews(
      id, GetContext());
}
