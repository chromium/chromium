// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_LINE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_LINE_VIEW_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class TabCollectionNode;
class TabGroupView;
class TabView;

// The view class for the tab group underline/side-line. It draws the colored
// group line for both horizontal and vertical tab strip orientations.
class TabGroupLineView : public views::View {
  METADATA_HEADER(TabGroupLineView, views::View)

 public:
  explicit TabGroupLineView(TabGroupView& tab_group_view);
  TabGroupLineView(const TabGroupLineView&) = delete;
  TabGroupLineView& operator=(const TabGroupLineView&) = delete;
  ~TabGroupLineView() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  void PaintVertical(gfx::Canvas* canvas, SkColor color);
  void PaintHorizontal(gfx::Canvas* canvas, SkColor color);

  SkPath GetHorizontalLinePath(const std::optional<SkPath>& active_tab_path,
                               float scale) const;
  SkPath GetOffsetOverlinePath(const TabView& tab_view, float scale) const;
  bool IsViewDragging(const views::View* view) const;
  std::optional<SkPath> GetActiveTabOverlinePath(const TabCollectionNode& node,
                                                 float scale) const;
  std::optional<SkPath> GetActiveSplitOverlinePath(
      const TabCollectionNode& split_node,
      float scale) const;
  std::optional<SkPath> GetActiveOverlinePath(float scale) const;

  const raw_ref<TabGroupView> tab_group_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_LINE_VIEW_H_
