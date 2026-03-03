// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SHARED_DROP_ARROW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SHARED_DROP_ARROW_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class ImageView;
}

// Used during a drop session of a url. Tracks the position of the drop as
// well as a widget used to highlight where the drop occurs.
class DropArrow : public views::WidgetObserver {
 public:
  // Used to represent the direction the arrow is pointing.
  enum class Direction {
    kDown,
    kUp,
    kLeft,
    kRight,
  };

  // Returns the bounds for the drop arrow at `index`. `direction` is set to
  // the direction the arrow should point.
  using BoundsCallback =
      base::RepeatingCallback<gfx::Rect(const BrowserRootView::DropIndex& index,
                                        Direction* direction)>;

  DropArrow(const BrowserRootView::DropIndex& index,
            gfx::NativeWindow context,
            BoundsCallback bounds_callback);
  DropArrow(const DropArrow&) = delete;
  DropArrow& operator=(const DropArrow&) = delete;
  ~DropArrow() override;

  // Returns the size of the arrow image. Height represents the length of the
  // arrow in the direction it points and width is the opposite dimension.
  static gfx::Size GetSize();

  void SetIndex(const BrowserRootView::DropIndex& index);
  BrowserRootView::DropIndex index() const { return index_; }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  void UpdateBounds();

  // Index of the tab to drop on.
  BrowserRootView::DropIndex index_;

  // Callback to get the bounds of the drop arrow for a given `DropIndex`.
  BoundsCallback bounds_callback_;

  // Direction the arrow should point in.
  std::optional<Direction> direction_;

  // Renders the drop indicator.
  raw_ptr<views::Widget> arrow_widget_ = nullptr;

  raw_ptr<views::ImageView> arrow_view_ = nullptr;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      scoped_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SHARED_DROP_ARROW_H_
