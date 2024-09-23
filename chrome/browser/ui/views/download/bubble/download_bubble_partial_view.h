// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PARTIAL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PARTIAL_VIEW_H_

#include <optional>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/download/download_bubble_row_list_view_info.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_primary_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/focus/focus_manager.h"

class Browser;
class DownloadBubbleUIController;
class DownloadBubbleNavigationHandler;

// This class encapsulates the "partial view" in the download bubble. This gives
// a compact representation of downloads that recently completed.
class DownloadBubblePartialView : public DownloadBubblePrimaryView,
                                  public views::FocusChangeListener {
  METADATA_HEADER(DownloadBubblePartialView, DownloadBubblePrimaryView)

 public:
  DownloadBubblePartialView(
      base::WeakPtr<Browser> browser,
      base::WeakPtr<DownloadBubbleUIController> bubble_controller,
      base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
      const DownloadBubbleRowListViewInfo& info,
      base::OnceClosure on_interacted_closure);
  DownloadBubblePartialView(const DownloadBubblePartialView&) = delete;
  DownloadBubblePartialView& operator=(const DownloadBubblePartialView&) =
      delete;
  ~DownloadBubblePartialView() override;

  // DownloadBubblePrimaryView:
  std::string_view GetVisibleTimeHistogramName() const override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  bool IsPartialView() const override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* before, views::View* now) override;
  void OnDidChangeFocus(views::View* before, views::View* now) override {}

 private:
  // Run the |on_interacted_closure_|.
  void OnInteracted();

  // A callback to be run when this view has been hovered over by the mouse or
  // focused by the keyboard.
  base::OnceClosure on_interacted_closure_;

  // Records the end time of the last download if it is successful.
  std::optional<base::Time> last_download_completed_time_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PARTIAL_VIEW_H_
