// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_BUTTON_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/bubble/download_display.h"
#include "chrome/browser/download/bubble/download_icon_state.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Browser;
class BrowserView;
class DownloadDisplayController;
class DownloadBubbleUIController;
class DownloadBubbleRowView;
class DownloadBubbleSecurityView;

class DownloadBubbleNavigationHandler {
 public:
  // Primary dialog is either main or partial view.
  virtual void OpenPrimaryDialog() = 0;
  virtual void OpenSecurityDialog(DownloadBubbleRowView* download_row_view) = 0;
  virtual void CloseDialog(views::Widget::ClosedReason reason) = 0;
  virtual void ResizeDialog() = 0;
};

// Download icon shown in the trusted area of the toolbar. Its lifetime is tied
// to that of its parent ToolbarView. The icon is made visible when downloads
// are in progress or when a download was initiated in the past 24 hours.
class DownloadToolbarButtonView : public ToolbarButton,
                                  public DownloadDisplay,
                                  public DownloadBubbleNavigationHandler {
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
  void HideDetails() override;
  bool IsShowingDetails() override;
  bool IsFullscreenWithParentViewHidden() override;

  // ToolbarButton:
  void UpdateIcon() override;
  void OnThemeChanged() override;

  // DownloadBubbleNavigationHandler:
  void OpenPrimaryDialog() override;
  void OpenSecurityDialog(DownloadBubbleRowView* download_row_view) override;
  void CloseDialog(views::Widget::ClosedReason reason) override;
  void ResizeDialog() override;

  DownloadBubbleUIController* bubble_controller() {
    return bubble_controller_.get();
  }

  DownloadDisplayController* display_controller() { return controller_.get(); }

  SkColor GetIconColor() const;
  void SetIconColor(SkColor color);

 private:
  // views::Button overrides:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  void ButtonPressed();
  void CreateBubbleDialogDelegate(std::unique_ptr<View> bubble_contents_view);
  void OnBubbleDelegateDeleted();

  // Get the primary view, which may be the full or the partial view.
  std::unique_ptr<View> GetPrimaryView();
  // Create a scrollable row list view for either the full or the partial view.
  std::unique_ptr<View> CreateRowListView(
      std::vector<DownloadUIModel::DownloadUIModelPtr> model_list);

  raw_ptr<Browser> browser_;
  bool is_primary_partial_view_ = false;
  // Controller for the DownloadToolbarButton UI.
  std::unique_ptr<DownloadDisplayController> controller_;
  // Controller for keeping track of items for both main view and partial view.
  std::unique_ptr<DownloadBubbleUIController> bubble_controller_;
  raw_ptr<views::BubbleDialogDelegate> bubble_delegate_ = nullptr;
  raw_ptr<View> primary_view_ = nullptr;
  raw_ptr<DownloadBubbleSecurityView> security_view_ = nullptr;

  // Override for the icon color. Used for PWAs, which don't have full
  // ThemeProvider color support.
  absl::optional<SkColor> icon_color_;

  gfx::SlideAnimation scanning_animation_{this};

  base::WeakPtrFactory<DownloadToolbarButtonView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_BUTTON_VIEW_H_
