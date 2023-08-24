// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_BUTTON_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/download/bubble/download_display.h"
#include "chrome/browser/download/bubble/download_icon_state.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace gfx {
class RenderText;
}

class Browser;
class BrowserView;
class DownloadDisplayController;
class DownloadBubbleContentsView;
class DownloadBubbleUIController;
class DownloadBubbleRowView;

class DownloadBubbleNavigationHandler {
 public:
  // Primary dialog is either main or partial view.
  virtual void OpenPrimaryDialog() = 0;
  virtual void OpenSecurityDialog(DownloadBubbleRowView* download_row_view) = 0;
  virtual void CloseDialog(views::Widget::ClosedReason reason) = 0;
  virtual void ResizeDialog() = 0;
  // Callback invoked when the dialog has been interacted with by hovering over
  // or by focusing (on the partial view).
  virtual void OnDialogInteracted() = 0;
  virtual base::WeakPtr<DownloadBubbleNavigationHandler> GetWeakPtr() = 0;
};

// Download icon shown in the trusted area of the toolbar. Its lifetime is tied
// to that of its parent ToolbarView. The icon is made visible when downloads
// are in progress or when a download was initiated in the past 1 hour.
// When there are multiple downloads, a circular badge in the corner of the icon
// displays the number of ongoing downloads.
class DownloadToolbarButtonView : public ToolbarButton,
                                  public DownloadDisplay,
                                  public DownloadBubbleNavigationHandler,
                                  public BrowserListObserver {
 public:
  METADATA_HEADER(DownloadToolbarButtonView);
  explicit DownloadToolbarButtonView(BrowserView* browser_view);
  DownloadToolbarButtonView(const DownloadToolbarButtonView&) = delete;
  DownloadToolbarButtonView& operator=(const DownloadToolbarButtonView&) =
      delete;
  ~DownloadToolbarButtonView() override;

  // DownloadDisplay implementation.
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  void Enable() override;
  void Disable() override;
  void UpdateDownloadIcon(bool show_animation) override;
  void ShowDetails() override;
  void HideDetails() override;
  bool IsShowingDetails() override;
  bool IsFullscreenWithParentViewHidden() override;
  bool ShouldShowExclusiveAccessBubble() override;

  // ToolbarButton:
  void UpdateIcon() override;
  void OnThemeChanged() override;
  void Layout() override;
  bool ShouldShowInkdropAfterIphInteraction() override;

  // DownloadBubbleNavigationHandler:
  void OpenPrimaryDialog() override;
  void OpenSecurityDialog(DownloadBubbleRowView* download_row_view) override;
  void CloseDialog(views::Widget::ClosedReason reason) override;
  void ResizeDialog() override;
  void OnDialogInteracted() override;
  base::WeakPtr<DownloadBubbleNavigationHandler> GetWeakPtr() override;

  // BrowserListObserver
  void OnBrowserSetLastActive(Browser* browser) override;

  // Deactivates the automatic closing of the partial bubble.
  void DeactivateAutoClose();

  DownloadBubbleUIController* bubble_controller() {
    return bubble_controller_.get();
  }

  DownloadDisplayController* display_controller() { return controller_.get(); }

  SkColor GetIconColor() const;
  void SetIconColor(SkColor color);

  void DisableAutoCloseTimerForTesting();
  void DisableDownloadStartedAnimationForTesting();

 private:
  // Max download count to show in the badge. Any higher number of downloads
  // results in a placeholder ("9+").
  static constexpr int kMaxDownloadCountDisplayed = 9;

  // views::Button overrides:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  gfx::ImageSkia GetBadgeImage(bool is_active,
                               int progress_download_count,
                               SkColor badge_text_color,
                               SkColor badge_background_color);
  // Returns an reference to the appropriate RenderText for the number of
  // downloads, to be used in rendering the badge. |progress_download_count|
  // should be at least 2.
  gfx::RenderText& GetBadgeText(int progress_download_count,
                                SkColor badge_text_color);

  void ButtonPressed();
  void CreateBubbleDialogDelegate();
  void OnBubbleClosing();

  // Callback invoked when the partial view is closed.
  void OnPartialViewClosed();

  // Creates a timer to track the auto-close task. Does not start the timer.
  void CreateAutoCloseTimer();

  // Called to automatically close the partial view, if such closing has not
  // been deactivated.
  void AutoClosePartialView();

  // Get the models for the primary view, which may be the full or the partial
  // view.
  std::vector<DownloadUIModel::DownloadUIModelPtr> GetPrimaryViewModels();

  // If |has_pending_download_started_animation_| is true, shows an animation of
  // a download icon moving upwards towards the toolbar icon.
  void ShowPendingDownloadStartedAnimation();

  SkColor GetProgressColor(bool is_disabled, bool is_active) const;

  raw_ptr<Browser> browser_;
  bool is_primary_partial_view_ = false;
  // Controller for the DownloadToolbarButton UI.
  std::unique_ptr<DownloadDisplayController> controller_;
  // Controller for keeping track of items for both main view and partial view.
  std::unique_ptr<DownloadBubbleUIController> bubble_controller_;
  raw_ptr<views::BubbleDialogDelegate> bubble_delegate_ = nullptr;
  raw_ptr<DownloadBubbleContentsView> bubble_contents_ = nullptr;

  // Marks whether there is a pending download started animation. This is needed
  // because the animation should only be triggered after the view has been
  // laid out properly, so this provides a way to remember to show the animation
  // if needed, when calling Layout().
  bool has_pending_download_started_animation_ = false;
  // Overrides whether we are allowed to show the download started animation,
  // may be false in tests.
  bool show_download_started_animation_ = true;

  // Tracks the task to automatically close the partial view after some amount
  // of time open, to minimize disruption to the user.
  std::unique_ptr<base::RetainingOneShotTimer> auto_close_bubble_timer_;
  // Whether we are allowed to create the above timer, may be false in tests.
  bool create_auto_close_timer_ = true;

  base::TimeTicks button_click_time_;

  // RenderTexts used for the number in the badge. Stores the text for "n" at
  // index n - 1, and stores the text for the placeholder ("9+") at index 0.
  // This is done to avoid re-creating the same RenderText on each paint. Text
  // color of each RenderText is reset upon each paint.
  std::array<std::unique_ptr<gfx::RenderText>, kMaxDownloadCountDisplayed>
      render_texts_{};
  // Badge view drawn on top of the rest of the children. It is positioned at
  // the bottom right corner of this view's bounds.
  raw_ptr<views::ImageView> badge_image_view_ = nullptr;

  // Maps number of in-progress downloads to the corresponding tooltip text, to
  // avoid having to create the strings repeatedly. The entry for 0 is the
  // default tooltip ("Downloads"), the entries for larger numbers are the
  // tooltips for N in-progress downloads ("N downloads in progress").
  std::map<int, std::u16string> tooltip_texts_;

  // Override for the icon color. Used for PWAs, which don't have full
  // ThemeProvider color support.
  absl::optional<SkColor> icon_color_;

  gfx::SlideAnimation scanning_animation_{this};

  // Used for holding the top views visible while the download bubble is showing
  // in immersive mode on ChromeOS and Mac.
  std::unique_ptr<ImmersiveRevealedLock> immersive_revealed_lock_;

  base::WeakPtrFactory<DownloadToolbarButtonView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_BUTTON_VIEW_H_
