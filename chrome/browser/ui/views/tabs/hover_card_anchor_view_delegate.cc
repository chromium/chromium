// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/hover_card_anchor_view_delegate.h"

#include "base/check_deref.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"

HoverCardAnchorViewDelegate::HoverCardAnchorViewDelegate(Observer* observer,
                                                         views::View* view)
    : view_(CHECK_DEREF(view)) {
  CHECK(observer);
  AddObserver(observer);
  view_observation_.Observe(view);
}

HoverCardAnchorViewDelegate::~HoverCardAnchorViewDelegate() = default;

// static
bool HoverCardAnchorViewDelegate::IsValidTargetView(const views::View* view) {
  if (view == nullptr) {
    return false;
  }

  if (const Tab* tab = views::AsViewClass<Tab>(view)) {
    // Never display a hover card for a closing tab.
    return !tab->closing();
  }
  return false;
}

bool HoverCardAnchorViewDelegate::HasValidTargetView(
    const TabStrip* tab_strip) const {
  if (HasTab()) {
    const Tab* tab = GetAsTab();
    // There are a bunch of conditions under which a tab may no longer be
    // valid, including no longer belonging to the same tabstrip, being
    // dragged or detached, or just not being visible. We need to be
    // vigilant about invalid tabs due to e.g. crbug.com/1295601.
    return tab_strip->GetModelIndexOf(tab).has_value() && !tab->closing() &&
           !tab->detached() && !tab->dragging() && tab->GetVisible();
  }
  return false;
}

bool HoverCardAnchorViewDelegate::HasTab() const {
  return IsViewClass<Tab>(&view());
}

Tab* HoverCardAnchorViewDelegate::GetAsTab() const {
  CHECK(HasTab());
  return AsViewClass<Tab>(&view());
}

TabRendererData HoverCardAnchorViewDelegate::GetTabData() const {
  return GetAsTab()->data();
}

bool HoverCardAnchorViewDelegate::IsObserving() const {
  return view_observation_.IsObserving();
}

void HoverCardAnchorViewDelegate::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view,
    bool visible) {
  // If visibility anywhere in the hierarchy changed to false, then the target
  // view is not visible, so treat it as if it is going away.
  if (!visible) {
    OnViewIsDeleting(observed_view);
  }
}

void HoverCardAnchorViewDelegate::OnViewIsDeleting(views::View* observed_view) {
  if (&view_.get() != observed_view) {
    return;
  }

  view_observation_.Reset();
  observers_.Notify(&Observer::OnAnchorViewRemoved);
}

void HoverCardAnchorViewDelegate::AddObserver(Observer* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void HoverCardAnchorViewDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}
