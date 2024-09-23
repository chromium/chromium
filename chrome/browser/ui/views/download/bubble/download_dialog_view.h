// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_DIALOG_VIEW_H_

#include <string_view>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/download/download_bubble_row_list_view_info.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_primary_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/link.h"

class Browser;

namespace views {
class Button;
class View;
}  // namespace views

// This view represents the 'main view' that is shown when the user clicks on
// the download toolbar button. Unlike the partial view, it does not
// automatically close. It also has a header and close button, as well as a
// footer with a link to chrome://downloads.
class DownloadDialogView : public DownloadBubblePrimaryView {
  METADATA_HEADER(DownloadDialogView, DownloadBubblePrimaryView)

 public:
  DownloadDialogView(const DownloadDialogView&) = delete;
  DownloadDialogView& operator=(const DownloadDialogView&) = delete;

  DownloadDialogView(
      base::WeakPtr<Browser> browser,
      base::WeakPtr<DownloadBubbleUIController> bubble_controller,
      base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
      const DownloadBubbleRowListViewInfo& info);
  ~DownloadDialogView() override;

  // DownloadBubblePrimaryView:
  // Returns the close button. The close button should be the initially focused
  // view to make it easier for the user to close the dialog.
  views::View* GetInitiallyFocusedView() override;
  bool IsPartialView() const override;

 private:
  // DownloadBubblePrimaryView
  std::string_view GetVisibleTimeHistogramName() const override;

  void CloseBubble();
  void ShowAllDownloads();
  void AddHeader();
  void AddFooter();

  base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler_;
  base::WeakPtr<Browser> browser_;
  raw_ptr<views::Button> close_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_DIALOG_VIEW_H_
