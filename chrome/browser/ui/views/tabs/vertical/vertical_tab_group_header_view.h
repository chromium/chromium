// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_GROUP_HEADER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_GROUP_HEADER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace tab_groups {
class TabGroupVisualData;
}

namespace views {
class Label;
}

// View for a tab group header in the vertical tabstrip.
class VerticalTabGroupHeaderView : public views::FlexLayoutView {
  METADATA_HEADER(VerticalTabGroupHeaderView, views::FlexLayoutView)

 public:
  explicit VerticalTabGroupHeaderView(
      const tab_groups::TabGroupVisualData* tab_group_visual_data,
      base::RepeatingCallback<void(ToggleTabGroupCollapsedStateOrigin)>
          toggle_collapsed_state_callback);
  VerticalTabGroupHeaderView(const VerticalTabGroupHeaderView&) = delete;
  VerticalTabGroupHeaderView& operator=(const VerticalTabGroupHeaderView&) =
      delete;
  ~VerticalTabGroupHeaderView() override;

  // views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  void OnDataChanged(
      const tab_groups::TabGroupVisualData* tab_group_visual_data);

 private:
  const raw_ptr<views::Label> group_header_label_ = nullptr;

  base::RepeatingCallback<void(ToggleTabGroupCollapsedStateOrigin)>
      toggle_collapsed_state_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_GROUP_HEADER_VIEW_H_
