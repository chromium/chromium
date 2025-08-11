// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/interaction/browser_elements_views.h"

#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/views/interaction/element_tracker_views.h"

DEFINE_FRAMEWORK_SPECIFIC_METADATA(BrowserElementsViews)

BrowserElementsViews::BrowserElementsViews(BrowserWindowInterface& browser)
    : BrowserElements(browser) {}

BrowserElementsViews::~BrowserElementsViews() = default;

// static
BrowserElementsViews* BrowserElementsViews::From(
    BrowserWindowInterface* browser) {
  auto* const base = BrowserElements::From(browser);
  return base ? base->AsA<BrowserElementsViews>() : nullptr;
}

void BrowserElementsViews::Init(views::View* view) {
  context_view_ = view;
}

void BrowserElementsViews::TearDown() {
  context_view_ = nullptr;
}

ui::ElementContext BrowserElementsViews::GetContext() {
  return views::ElementTrackerViews::GetContextForView(context_view_);
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
