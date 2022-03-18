// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_BUTTON_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/bubble/download_display.h"
#include "chrome/browser/download/bubble/download_icon_state.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Browser;
class BrowserView;
class DownloadDisplayController;
class DownloadBubbleUIController;

// Download icon shown in the trusted area of the toolbar. Its lifetime is tied
// to that of its parent ToolbarView. The icon is made visible when downloads
// are in progress or when a download was initiated in the past 24 hours.
class DownloadToolbarButtonView : public ToolbarButton, public DownloadDisplay {
 public:
  METADATA_HEADER(DownloadToolbarButtonView);
  explicit DownloadToolbarButtonView(BrowserView* browser_view);
  DownloadToolbarButtonView(const DownloadToolbarButtonView&) = delete;
  DownloadToolbarButtonView& operator=(const DownloadToolbarButtonView&) =
      delete;
  ~DownloadToolbarButtonView() override;

  // DownloadsDisplay implementation.
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  void Enable() override;
  void Disable() override;
  void UpdateDownloadIcon() override;
  void ShowDetails() override;

  // ToolbarButton:
  void UpdateIcon() override;

 private:
  // views::Button overrides:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  void ButtonPressed();
  std::unique_ptr<views::BubbleDialogDelegate> CreateBubbleDialogDelegate(
      std::unique_ptr<View> bubble_contents_view);
  void OnBubbleDelegateDeleted();

  raw_ptr<Browser> browser_;
  // Controller for the DownloadToolbarButton UI.
  std::unique_ptr<DownloadDisplayController> controller_;
  // Controller for keeping track of items for both main view and partial view.
  std::unique_ptr<DownloadBubbleUIController> bubble_controller_;
  raw_ptr<views::BubbleDialogDelegate> bubble_delegate_ = nullptr;

  base::WeakPtrFactory<DownloadToolbarButtonView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_BUTTON_VIEW_H_
