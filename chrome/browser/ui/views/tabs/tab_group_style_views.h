// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_STYLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_STYLE_VIEWS_H_

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/geometry/rect.h"

namespace views {
class Background;
class View;
}  // namespace views

class TabGroupHeader;
class TabGroupViews;

// Styling of tab groups that depends on views.
class TabGroupStyleViews {
 public:
  explicit TabGroupStyleViews(const TabGroupViews& tab_group_views);
  TabGroupStyleViews(const TabGroupStyleViews&) = delete;
  TabGroupStyleViews& operator=(const TabGroupStyleViews&) = delete;
  virtual ~TabGroupStyleViews();

  // returns whether the underline for the group should be hidden
  virtual bool TabGroupUnderlineShouldBeHidden() const;
  virtual bool TabGroupUnderlineShouldBeHidden(
      const views::View* leading_view,
      const views::View* trailing_view) const;
  // Returns the path of an underline given the local bounds of the underline.
  virtual SkPath GetUnderlinePath(gfx::Rect local_bounds) const;

  // Returns the bounds of a title chip without any text.
  virtual gfx::Rect GetEmptyTitleChipBounds(const TabGroupHeader* header) const;

  // Returns the background of a title chip without any text.
  virtual std::unique_ptr<views::Background> GetEmptyTitleChipBackground(
      SkColor color) const;

  // Returns the radius for the tab group header's highlight path. This is used
  // when the header is focused.
  virtual int GetHighlightPathGeneratorCornerRadius(
      const views::View* title) const;

  // While calculating desired width of a tab group an adjustment value is added
  // for the distance between the tab group header and the right tab.
  virtual int GetTitleAdjustmentToTabGroupHeaderDesiredWidth(
      std::u16string title) const;

  // Returns the sync icon width.
  virtual float GetSyncIconWidth() const;

  // Returns the attention indicator icon width.
  virtual float GetAttentionIndicatorWidth() const;

  // Overlap between the tab group view and neighbor tab slot
  virtual int GetTabGroupViewOverlap() const;

 protected:
  const raw_ref<const TabGroupViews> tab_group_views_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_STYLE_VIEWS_H_
