// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_CONTENTS_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/download_ui_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class Browser;
class DownloadBubbleNavigationHandler;
class DownloadBubblePrimaryView;
class DownloadBubbleRowView;
class DownloadBubbleSecurityView;
class DownloadBubbleUIController;

namespace views {
class BubbleDialogDelegate;
}  // namespace views

// View that contains the contents of the download bubble. Owns and allows
// switching between a primary page (either the "main" or "partial" view,
// containing the download item rows), or the security page (which shows
// warnings if applicable). Always opens up to the primary view by default,
// before possibly being switched to the security view.
class DownloadBubbleContentsView : public views::View {
 public:
  // Types of pages that this view can show.
  enum class Page {
    kPrimary,
    kSecurity,
  };

  METADATA_HEADER(DownloadBubbleContentsView);

  DownloadBubbleContentsView(
      base::WeakPtr<Browser> browser,
      base::WeakPtr<DownloadBubbleUIController> bubble_controller,
      base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
      // Whether the primary view is the partial view.
      bool primary_view_is_partial_view,
      // Models for rows that should go in the primary view. Must not be empty.
      std::vector<DownloadUIModel::DownloadUIModelPtr> primary_view_models,
      // The owning bubble's delegate.
      views::BubbleDialogDelegate* bubble_delegate);
  ~DownloadBubbleContentsView() override;

  DownloadBubbleContentsView(const DownloadBubbleContentsView&) = delete;
  DownloadBubbleContentsView& operator=(const DownloadBubbleContentsView&) =
      delete;

  // Switches to the requested page by showing the page and hiding all other
  // pages.
  void ShowPage(Page page);

  // Which page is currently visible.
  Page VisiblePage() const;

  // Forwards to `security_view_`. (Does not switch to the security view.)
  void UpdateSecurityView(DownloadBubbleRowView* row);

  // Gets the row view at the given index.
  DownloadBubbleRowView* GetPrimaryViewRowForTesting(size_t index);

 private:
  // Switches to the page that should currently be showing.
  void SwitchToCurrentPage();

  // May be a DownloadBubblePartialView or a DownloadDialogView (main view).
  raw_ptr<DownloadBubblePrimaryView> primary_view_ = nullptr;
  // The security view is hidden by default but may be switched to.
  raw_ptr<DownloadBubbleSecurityView> security_view_ = nullptr;

  // The currently visible page.
  Page page_ = Page::kPrimary;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_CONTENTS_VIEW_H_
