// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_views.h"

#include <utility>

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_highlight.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

TabGroupViews::TabGroupViews(TabStrip* tab_strip,
                             const tab_groups::TabGroupId& group)
    : tab_strip_(tab_strip), group_(group) {
  header_ = tab_strip_->AddChildView(
      std::make_unique<TabGroupHeader>(tab_strip_, group_));
  underline_ = tab_strip_->AddChildView(
      std::make_unique<TabGroupUnderline>(this, group_));
  highlight_ = tab_strip_->AddChildView(
      std::make_unique<TabGroupHighlight>(this, group_));
}

TabGroupViews::~TabGroupViews() {
  tab_strip_->RemoveChildViewT(header_);
  tab_strip_->RemoveChildViewT(underline_);
  tab_strip_->RemoveChildViewT(highlight_);
}

void TabGroupViews::UpdateBounds() {
  const gfx::Rect bounds = GetBounds();
  underline_->UpdateBounds(bounds);
  highlight_->SetBoundsRect(bounds);
}

void TabGroupViews::OnGroupVisualsChanged() {
  header_->VisualsChanged();
  underline_->SchedulePaint();
  const int active_index = tab_strip_->controller()->GetActiveIndex();
  if (active_index != TabStripModel::kNoTab)
    tab_strip_->tab_at(active_index)->SchedulePaint();
}

gfx::Rect TabGroupViews::GetBounds() const {
  gfx::Rect bounds = header_->bounds();

  // If the group is (done animating to) collapsed, the tabs will be stacked at
  // the right edge of the header, so this is a no-op. But if the group is mid
  // collapse animation, this will set the header bounds correctly.
  const Tab* last_tab = GetLastTabInGroup();
  if (last_tab) {
    const int width = last_tab->bounds().right() - bounds.x();
    if (width > 0)
      bounds.set_width(width);
  }
  return bounds;
}

const Tab* TabGroupViews::GetLastTabInGroup() const {
  const base::Optional<int> last_tab =
      tab_strip_->controller()->GetLastTabInGroup(group_);
  return last_tab.has_value() ? tab_strip_->tab_at(last_tab.value()) : nullptr;
}

SkColor TabGroupViews::GetGroupColor() const {
  return tab_strip_->GetPaintedGroupColor(
      tab_strip_->controller()->GetGroupColorId(group_));
}

SkColor TabGroupViews::GetTabBackgroundColor() const {
  return tab_strip_->GetTabBackgroundColor(
      TabActive::kInactive, BrowserFrameActiveState::kUseCurrent);
}

SkColor TabGroupViews::GetGroupBackgroundColor() const {
  const SkColor active_color = tab_strip_->GetTabBackgroundColor(
      TabActive::kActive, BrowserFrameActiveState::kUseCurrent);
  return SkColorSetA(active_color, gfx::Tween::IntValueBetween(
                                       TabStyle::kSelectedTabOpacity,
                                       SK_AlphaTRANSPARENT, SK_AlphaOPAQUE));
}
