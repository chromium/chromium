// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_WEB_VIEW_H_

#include "chrome/browser/download/download_shelf.h"
#include "ui/views/controls/webview/webview.h"

class Browser;

class DownloadShelfWebView : public DownloadShelf, public views::WebView {
 public:
  explicit DownloadShelfWebView(Browser* browser);
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

 private:
  // DownloadShelf:
  bool IsShowing() const override;
  bool IsClosing() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_WEB_VIEW_H_
