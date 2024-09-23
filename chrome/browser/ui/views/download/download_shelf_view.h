// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_VIEW_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/download_shelf.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/mouse_watcher_view_host.h"

class Browser;
class BrowserView;
class DownloadItemView;

namespace views {
class ImageButton;
class MdTextButton;
}

// DownloadShelfView is a view that contains individual views for each download,
// as well as a close button and a link to show all downloads.
//
// DownloadShelfView does not hold an infinite number of download views, rather
// it'll automatically remove views once a certain point is reached.
class DownloadShelfView : public DownloadShelf,
                          public views::AccessiblePaneView,
                          public views::AnimationDelegateViews,
                          public views::MouseWatcherListener {
  METADATA_HEADER(DownloadShelfView, views::AccessiblePaneView)

 public:
  DownloadShelfView(Browser* browser, BrowserView* parent);
  DownloadShelfView(const DownloadShelfView&) = delete;
  DownloadShelfView& operator=(const DownloadShelfView&) = delete;
  ~DownloadShelfView() override;

  // DownloadShelf:
  bool IsShowing() const override;
  bool IsClosing() const override;

  views::View* GetView() override;

  // views::AccessiblePaneView:
  // TODO(crbug.com/40648316): Replace these with a LayoutManager
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& /*available_size*/) const override;
  void Layout(PassKey) override;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // views::MouseWatcherListener:
  void MouseMovedOutOfHost() override;

  // Sent from the DownloadItemView when the user opens an item.
  void AutoClose();

  // Removes a specified download view. The supplied view is deleted after
  // it's removed.
  void RemoveDownloadView(views::View* view);

  // Updates |button| according to the active theme.
  void ConfigureButtonForTheme(views::MdTextButton* button);

  DownloadItemView* GetViewOfLastDownloadItemForTesting();

 protected:
  // DownloadShelf:
  void DoShowDownload(DownloadUIModel::DownloadUIModelPtr download) override;
  void DoOpen() override;
  void DoClose() override;
  void DoHide() override;
  void DoUnhide() override;

  // views::AccessiblePaneView:
  void OnPaintBorder(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;
  views::View* GetDefaultFocusableChild() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DownloadShelfViewTest, ShowAllViewColors);

  // The animation for adding new items to the shelf.
  gfx::SlideAnimation new_item_animation_{this};

  // The show/hide animation for the shelf itself.
  gfx::SlideAnimation shelf_animation_{this};

  // The download views. These are also child Views, and deleted when
  // the DownloadShelfView is deleted.
  // TODO(pkasting): Remove this in favor of making these the children of a
  // nested view, so they can easily be laid out and iterated.
  std::vector<raw_ptr<DownloadItemView, VectorExperimental>> download_views_;

  // Button for showing all downloads (chrome://downloads).
  raw_ptr<views::MdTextButton> show_all_view_;

  // Button for closing the downloads. This is contained as a child, and
  // deleted by View.
  raw_ptr<views::ImageButton> close_button_;

  // Hidden view that will contain status text for immediate output by
  // screen readers.
  raw_ptr<views::View> accessible_alert_;

  // The window this shelf belongs to.
  raw_ptr<BrowserView> parent_;

  views::MouseWatcher mouse_watcher_{
      std::make_unique<views::MouseWatcherViewHost>(this, gfx::Insets()), this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_VIEW_H_
