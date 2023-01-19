// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_STYLE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_STYLE_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

class TabGroupViews;

class TabGroupStyle {
 public:
  explicit TabGroupStyle(const TabGroupViews& tab_group_views);
  TabGroupStyle(const TabGroupStyle&) = delete;
  TabGroupStyle& operator=(const TabGroupStyle&) = delete;
  virtual ~TabGroupStyle();
  // returns whether the underline for the group should be hidden
  virtual bool TabGroupUnderlineShouldBeHidden() const;
  virtual bool TabGroupUnderlineShouldBeHidden(
      const views::View* leading_view,
      const views::View* trailing_view) const;
  // Returns the path of an underline given the local bounds of the underline.
  virtual SkPath GetUnderlinePath(gfx::Rect local_bounds) const;

 protected:
  const raw_ref<const TabGroupViews> tab_group_views_;
};

class ChromeRefresh2023TabGroupStyle : public TabGroupStyle {
 public:
  explicit ChromeRefresh2023TabGroupStyle(const TabGroupViews& tab_group_views);
  ChromeRefresh2023TabGroupStyle(const ChromeRefresh2023TabGroupStyle&) =
      delete;
  ChromeRefresh2023TabGroupStyle& operator=(
      const ChromeRefresh2023TabGroupStyle&) = delete;
  ~ChromeRefresh2023TabGroupStyle() override;

  bool TabGroupUnderlineShouldBeHidden() const override;
  bool TabGroupUnderlineShouldBeHidden(
      const views::View* leading_view,
      const views::View* trailing_view) const override;
  SkPath GetUnderlinePath(gfx::Rect local_bounds) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_STYLE_H_
