// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_WEB_VIEW_H_

#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/ui/views/download/download_shelf_context_menu_view.h"
#include "chrome/browser/ui/webui/download_shelf/download_shelf_ui.h"
#include "chrome/browser/ui/webui/download_shelf/download_shelf_ui_embedder.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/webview/webview.h"

class Browser;
class BrowserView;

class DownloadShelfWebView : public DownloadShelf,
                             public DownloadShelfUIEmbedder,
                             public views::WebView,
                             public views::AnimationDelegateViews {
 public:
  DownloadShelfWebView(Browser* browser, BrowserView* parent);
  DownloadShelfWebView(const DownloadShelfWebView&) = delete;
  DownloadShelfWebView& operator=(const DownloadShelfWebView&) = delete;
  ~DownloadShelfWebView() override;

  // views::WebView:
  gfx::Size CalculatePreferredSize() const override;

 protected:
  // DownloadShelf:
  void DoShowDownload(DownloadUIModel::DownloadUIModelPtr download) override;
  void DoOpen() override;
  void DoClose() override;
  void DoHide() override;
  void DoUnhide() override;
  views::View* GetView() override;

  // DownloadShelfUIEmbedder:
  void ShowDownloadContextMenu(DownloadUIModel* download,
                               const gfx::Point& position) override;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // views::WebView:
  void OnThemeChanged() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DownloadShelfWebViewTest, VisibilityTest);

  // DownloadShelf:
  bool IsShowing() const override;
  bool IsClosing() const override;

  DownloadShelfUI* GetDownloadShelfUI();

  BrowserView* parent_;

  // The show/hide animation for the shelf itself.
  gfx::SlideAnimation shelf_animation_{this};

  std::unique_ptr<DownloadShelfContextMenuView> context_menu_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_WEB_VIEW_H_
