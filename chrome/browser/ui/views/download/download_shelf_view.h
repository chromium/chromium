// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_VIEW_H_

#include <stddef.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/download/download_shelf.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/mouse_watcher.h"

class Browser;
class BrowserView;
class DownloadItemView;

namespace content {
class PageNavigator;
}

namespace views {
class ImageButton;
class MdTextButton;
}

// DownloadShelfView is a view that contains individual views for each download,
// as well as a close button and a link to show all downloads.
//
// DownloadShelfView does not hold an infinite number of download views, rather
// it'll automatically remove views once a certain point is reached.
class DownloadShelfView : public views::AccessiblePaneView,
                          public views::AnimationDelegateViews,
                          public DownloadShelf,
                          public views::ButtonListener,
                          public views::LinkListener,
                          public views::MouseWatcherListener {
 public:
  DownloadShelfView(Browser* browser, BrowserView* parent);
  ~DownloadShelfView() override;
  // Sent from the DownloadItemView when the user opens an item.
  void OpenedDownload();

  // Returns the relevant containing object that can load pages.
  // i.e. the |browser_|.
  content::PageNavigator* GetNavigator();

  // Returns the parent_.
  BrowserView* get_parent() { return parent_; }

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;

  // views::AnimationDelegateViews.
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // views::LinkListener.
  // Invoked when the user clicks the 'show all downloads' link button.
  void LinkClicked(views::Link* source, int event_flags) override;

  // views::ButtonListener:
  // Invoked when the user clicks the close button. Asks the browser to
  // hide the download shelf.
  void ButtonPressed(views::Button* button, const ui::Event& event) override;

  // DownloadShelf:
  bool IsShowing() const override;
  bool IsClosing() const override;
  Browser* browser() const override;

  // views::MouseWatcherListener:
  void MouseMovedOutOfHost() override;

  // Removes a specified download view. The supplied view is deleted after
  // it's removed.
  void RemoveDownloadView(views::View* view);

  // Updates |button| according to the active theme.
  void ConfigureButtonForTheme(views::MdTextButton* button);

 protected:
  // views::View:
  void AddedToWidget() override;
  void OnThemeChanged() override;

  // DownloadShelf:
  void DoAddDownload(DownloadUIModel::DownloadUIModelPtr download) override;
  void DoOpen() override;
  void DoClose(CloseReason reason) override;
  void DoHide() override;
  void DoUnhide() override;

  // views::AccessiblePaneView:
  views::View* GetDefaultFocusableChild() override;

 private:
  // Max number of download views we'll contain. Any time a view is added and
  // we already have this many download views, one is removed.
  static constexpr size_t kMaxDownloadViews = 15;

  // Padding from left edge and first download view.
  static constexpr int kStartPadding = 4;

  // Padding from right edge and close button/show downloads link.
  static constexpr int kEndPadding = 6;

  // Padding between the show all link and close button.
  static constexpr int kCloseAndLinkPadding = 6;

  // Adds a View representing a download to this DownloadShelfView.
  // DownloadShelfView takes ownership of the View, and will delete it as
  // necessary.
  void AddDownloadView(DownloadItemView* view);

  // Paints the border.
  void OnPaintBorder(gfx::Canvas* canvas) override;

  // Returns true if the shelf is wide enough to show the first download item.
  bool CanFitFirstDownloadItem();

  // Called on theme change.
  void UpdateColorsFromTheme();

  // Called when the "close shelf" animation ended.
  void Closed();

  // Returns true if we can auto close. We can auto-close if all the items on
  // the shelf have been opened.
  bool CanAutoClose();

  // Returns the color of text for the shelf (used for deriving icon color).
  SkColor GetTextColorForIconMd();

  // The browser for this shelf.
  Browser* const browser_;

  // The animation for adding new items to the shelf.
  gfx::SlideAnimation new_item_animation_;

  // The show/hide animation for the shelf itself.
  gfx::SlideAnimation shelf_animation_;

  // The download views. These are also child Views, and deleted when
  // the DownloadShelfView is deleted.
  std::vector<DownloadItemView*> download_views_;

  // Button for showing all downloads (chrome://downloads).
  views::MdTextButton* show_all_view_ = nullptr;

  // Button for closing the downloads. This is contained as a child, and
  // deleted by View.
  views::ImageButton* close_button_;

  // Hidden view that will contain status text for immediate output by
  // screen readers.
  views::View* accessible_alert_;

  // The window this shelf belongs to.
  BrowserView* parent_;

  views::MouseWatcher mouse_watcher_;

  DISALLOW_COPY_AND_ASSIGN(DownloadShelfView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_VIEW_H_
