// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_CONTENTS_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/download/download_bubble_contents_view_info.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_security_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class Browser;
class DownloadBubbleNavigationHandler;
class DownloadBubblePrimaryView;
class DownloadBubbleRowView;
class DownloadBubbleSecurityView;
class DownloadBubbleUIController;

namespace offline_items_collection {
struct ContentId;
}

namespace views {
class BubbleDialogDelegate;
}  // namespace views

// View that contains the contents of the download bubble. Owns and allows
// switching between a primary page (either the "main" or "partial" view,
// containing the download item rows), or the security page (which shows
// warnings if applicable). Always opens up to the primary view by default,
// before possibly being switched to the security view.
class DownloadBubbleContentsView : public views::View,
                                   public DownloadBubbleSecurityView::Delegate {
  METADATA_HEADER(DownloadBubbleContentsView, views::View)

 public:
  // Types of pages that this view can show.
  enum class Page {
    kPrimary,
    kSecurity,
  };

  DownloadBubbleContentsView(
      base::WeakPtr<Browser> browser,
      base::WeakPtr<DownloadBubbleUIController> bubble_controller,
      base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
      // Whether the primary view is the partial view.
      bool primary_view_is_partial_view,
      std::unique_ptr<DownloadBubbleContentsViewInfo> info,
      // The owning bubble's delegate.
      views::BubbleDialogDelegate* bubble_delegate);
  ~DownloadBubbleContentsView() override;

  DownloadBubbleContentsView(const DownloadBubbleContentsView&) = delete;
  DownloadBubbleContentsView& operator=(const DownloadBubbleContentsView&) =
      delete;

  // Shows the primary page. If `id` is supplied, looks for the row with the
  // given id, and if it is found, scrolls the primary view to that row and
  // returns a pointer to that row. Returns nullptr if the row was not found,
  // or if no id was supplied.
  DownloadBubbleRowView* ShowPrimaryPage(
      std::optional<offline_items_collection::ContentId> id = std::nullopt);

  // Initializes security page for the download with the given id, and switches
  // to it. `id` must refer to a valid download with a row in the primary view.
  void ShowSecurityPage(const offline_items_collection::ContentId& id);

  // Which page is currently visible.
  Page VisiblePage() const;

  // DownloadBubbleSecurityView::Delegate
  void ProcessSecuritySubpageButtonPress(
      const offline_items_collection::ContentId& id,
      DownloadCommands::Command command) override;
  void AddSecuritySubpageWarningActionEvent(
      const offline_items_collection::ContentId& id,
      DownloadItemWarningData::WarningAction action) override;
  void ProcessDeepScanPress(
      const ContentId& id,
      DownloadItemWarningData::DeepScanTrigger trigger,
      base::optional_ref<const std::string> password) override;
  void ProcessLocalDecryptionPress(
      const offline_items_collection::ContentId& id,
      base::optional_ref<const std::string> password) override;
  void ProcessLocalPasswordInProgressClick(
      const offline_items_collection::ContentId& id,
      DownloadCommands::Command command) override;
  bool IsEncryptedArchive(const ContentId& id) override;
  bool HasPreviousIncorrectPassword(const ContentId& id) override;

  // Gets the row view at the given index.
  DownloadBubbleRowView* GetPrimaryViewRowForTesting(size_t index);

  DownloadBubblePrimaryView* primary_view_for_testing() {
    return primary_view_;
  }

  DownloadBubbleSecurityView* security_view_for_testing() {
    return security_view_;
  }

  DownloadBubbleContentsViewInfo& info() { return *info_; }

 private:
  void InitializeSecurityView(const offline_items_collection::ContentId& id);

  // Gets the model from the row view in the primary view for the download with
  // given id. Returns nullptr if not found.
  DownloadUIModel* GetDownloadModel(
      const offline_items_collection::ContentId& id);

  std::unique_ptr<DownloadBubbleContentsViewInfo> info_;

  base::WeakPtr<DownloadBubbleUIController> bubble_controller_;
  base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler_;

  // TODO(crbug.com/40232473): The delegate should outlive the views.
  // Currently, the delegate is deleted in OnNativeWidetDestroyed(),
  // invalidating this pointer before this view is destroyed.
  raw_ptr<views::BubbleDialogDelegate, DanglingUntriaged> bubble_delegate_;

  // May be a DownloadBubblePartialView or a DownloadDialogView (main view).
  raw_ptr<DownloadBubblePrimaryView> primary_view_ = nullptr;
  // The security view is hidden by default but may be switched to.
  raw_ptr<DownloadBubbleSecurityView> security_view_ = nullptr;

  // The currently visible page.
  Page page_ = Page::kPrimary;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_CONTENTS_VIEW_H_
