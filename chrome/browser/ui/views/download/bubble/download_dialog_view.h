// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/link.h"
#include "ui/views/view.h"

class Browser;

class DownloadDialogView : public views::View {
 public:
  METADATA_HEADER(DownloadDialogView);
  DownloadDialogView(const DownloadDialogView&) = delete;
  DownloadDialogView& operator=(const DownloadDialogView&) = delete;

  DownloadDialogView(raw_ptr<Browser> browser,
                     std::unique_ptr<views::View> row_list_scroll_view,
                     DownloadBubbleNavigationHandler* navigation_handler);

 private:
  void CloseBubble();
  void ShowAllDownloads();
  void AddHeader();
  void AddFooter();

  // views::View.
  void OnThemeChanged() override;

  raw_ptr<DownloadBubbleNavigationHandler> navigation_handler_ = nullptr;
  raw_ptr<Browser> browser_ = nullptr;
  raw_ptr<views::Link> footer_link_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_DIALOG_VIEW_H_
