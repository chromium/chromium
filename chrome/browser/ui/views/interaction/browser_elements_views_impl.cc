// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/interaction/browser_elements_views_impl.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"

DEFINE_FRAMEWORK_SPECIFIC_METADATA_SUBCLASS(BrowserElementsViewsImpl,
                                            BrowserElementsViews)

BrowserElementsViewsImpl::BrowserElementsViewsImpl(
    BrowserWindowInterface& browser)
    : BrowserElementsViews(browser) {}

BrowserElementsViewsImpl::~BrowserElementsViewsImpl() = default;

void BrowserElementsViewsImpl::Init(views::View* view) {
  context_view_ = view;
}

void BrowserElementsViewsImpl::TearDown() {
  BrowserElementsViews::TearDown();
  context_view_ = nullptr;
}

ui::ElementContext BrowserElementsViewsImpl::GetContext() {
  return context_view_
             ? views::ElementTrackerViews::GetContextForView(context_view_)
             : ui::ElementContext();
}

views::Widget* BrowserElementsViewsImpl::GetPrimaryWindowWidget() {
  return context_view_ ? context_view_->GetWidget() : nullptr;
}

bool BrowserElementsViewsImpl::IsInitialized() const {
  return context_view_ != nullptr;
}
