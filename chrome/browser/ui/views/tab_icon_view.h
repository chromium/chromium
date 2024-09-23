// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_ICON_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/menu_button.h"

class TabIconViewModel;

namespace gfx {
class ImageSkia;
}

// A view to display a tab favicon or a throbber.
class TabIconView : public views::MenuButton {
  METADATA_HEADER(TabIconView, views::MenuButton)

 public:
  TabIconView();
  TabIconView(const TabIconView&) = delete;
  TabIconView& operator=(const TabIconView&) = delete;
  ~TabIconView() override;

  void SetModel(TabIconViewModel* model);

  // Invoke whenever the tab state changes or the throbber should update.
  void Update();

 private:
  // views::MenuButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

  void PaintThrobber(gfx::Canvas* canvas);
  void PaintFavicon(gfx::Canvas* canvas, const gfx::ImageSkia& image);

  // Our model.
  raw_ptr<TabIconViewModel> model_;

  // Time we painted the first frame of the current throbber animation, or
  // 0 if not painting the throbber.
  base::TimeTicks throbber_start_time_;
};

BEGIN_VIEW_BUILDER(/* no export */, TabIconView, views::MenuButton)
VIEW_BUILDER_PROPERTY(TabIconViewModel*, Model)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, TabIconView)

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_ICON_VIEW_H_
