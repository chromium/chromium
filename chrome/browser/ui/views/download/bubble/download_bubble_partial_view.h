// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PARTIAL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PARTIAL_VIEW_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "chrome/browser/download/download_ui_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class Browser;
class DownloadBubbleUIController;
class DownloadBubbleNavigationHandler;

// This class encapsulates the "partial view" in the download bubble. This gives
// a compact representation of downloads that recently completed.
class DownloadBubblePartialView : public views::View {
 public:
  METADATA_HEADER(DownloadBubblePartialView);

  static std::unique_ptr<DownloadBubblePartialView> Create(
      Browser* browser,
      DownloadBubbleUIController* bubble_controller,
      DownloadBubbleNavigationHandler* navigation_handler,
      std::vector<DownloadUIModel::DownloadUIModelPtr> rows,
      base::OnceClosure on_mouse_entered_closure);

  DownloadBubblePartialView(const DownloadBubblePartialView&) = delete;
  DownloadBubblePartialView& operator=(const DownloadBubblePartialView&) =
      delete;
  ~DownloadBubblePartialView() override;

  // views::View
  void OnMouseEntered(const ui::MouseEvent& event) override;

 private:
  DownloadBubblePartialView(
      Browser* browser,
      DownloadBubbleUIController* bubble_controller,
      DownloadBubbleNavigationHandler* navigation_handler,
      std::vector<DownloadUIModel::DownloadUIModelPtr> rows,
      base::OnceClosure on_mouse_entered_closure);

  base::OnceClosure on_mouse_entered_closure_;
};

#endif
