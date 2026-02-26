// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SHARED_DROP_ARROW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SHARED_DROP_ARROW_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class ImageView;
}

// Used during a drop session of a url. Tracks the position of the drop as
// well as a window used to highlight where the drop occurs.
class DropArrow : public views::WidgetObserver {
 public:
  DropArrow(const BrowserRootView::DropIndex& index,
            bool point_down,
            views::Widget* context);
  DropArrow(const DropArrow&) = delete;
  DropArrow& operator=(const DropArrow&) = delete;
  ~DropArrow() override;

  static gfx::Size GetSize();

  void set_index(const BrowserRootView::DropIndex& index) { index_ = index; }
  BrowserRootView::DropIndex index() const { return index_; }

  void SetPointDown(bool down);
  bool point_down() const { return point_down_; }

  void SetWindowBounds(const gfx::Rect& bounds);

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  // Index of the tab to drop on.
  BrowserRootView::DropIndex index_;

  // Direction the arrow should point in. If true, the arrow is displayed
  // above the tab and points down. If false, the arrow is displayed beneath
  // the tab and points up.
  bool point_down_ = false;

  // Renders the drop indicator.
  raw_ptr<views::Widget, DanglingUntriaged> arrow_window_ = nullptr;

  raw_ptr<views::ImageView, DanglingUntriaged> arrow_view_ = nullptr;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      scoped_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SHARED_DROP_ARROW_H_
