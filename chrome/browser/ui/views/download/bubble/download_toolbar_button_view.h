// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_BUTTON_VIEW_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/download/download_bubble_row_list_view_info.h"
#include "chrome/browser/ui/download/download_display.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace gfx {
class RenderText;
}

namespace offline_items_collection {
struct ContentId;
}

namespace views {
class EventMonitor;
}

class Browser;
class BrowserView;
class DownloadDisplayController;
class DownloadBubbleContentsView;
class DownloadBubbleRowView;
class DownloadBubbleUIController;

class DownloadBubbleNavigationHandler {
 public:
  using CloseOnDeactivatePin =
      views::BubbleDialogDelegate::CloseOnDeactivatePin;

  // Primary dialog is either main or partial view.
  virtual void OpenPrimaryDialog() = 0;

  // Opens the security dialog. If the bubble is not currently open, it creates
  // a new bubble to do so.
  virtual void OpenSecurityDialog(
      const offline_items_collection::ContentId& content_id) = 0;

  virtual void CloseDialog(views::Widget::ClosedReason reason) = 0;

  // Callback invoked when the dialog has been interacted with by hovering over
  // or by focusing (on the partial view).
  virtual void OnDialogInteracted() = 0;

  virtual void OnSecurityDialogButtonPress(
      const DownloadUIModel& model,
      DownloadCommands::Command command) = 0;

  // Returns a CloseOnDeactivatePin for the download bubble. For the lifetime of
  // the returned pin (if non-null), the download bubble will not close on
  // deactivate. Returns nullptr if the bubble is not open.
  virtual std::unique_ptr<CloseOnDeactivatePin>
  PreventDialogCloseOnDeactivate() = 0;

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
                                  public BrowserListObserver,
                                  public DownloadBubbleRowListViewInfoObserver {
  METADATA_HEADER(DownloadToolbarButtonView, ToolbarButton)

 public:
  // Identifies the bubble dialog widget for testing.
  static constexpr char kBubbleName[] = "DownloadBubbleDialog";

  explicit DownloadToolbarButtonView(BrowserView* browser_view);
  DownloadToolbarButtonView(const DownloadToolbarButtonView&) = delete;
  DownloadToolbarButtonView& operator=(const DownloadToolbarButtonView&) =
      delete;
  ~DownloadToolbarButtonView() override;

  // DownloadDisplay implementation.
  void Show() override;
  void Hide() override;
  bool IsShowing() const override;
  void Enable() override;
  void Disable() override;
  void UpdateDownloadIcon(const IconUpdateInfo& updates) override;
  void ShowDetails() override;
  void HideDetails() override;
  bool IsShowingDetails() const override;
  void AnnounceAccessibleAlertNow(const std::u16string& alert_text) override;
  bool IsFullscreenWithParentViewHidden() const override;
  bool ShouldShowExclusiveAccessBubble() const override;
  void OpenSecuritySubpage(
      const offline_items_collection::ContentId& id) override;
  bool OpenMostSpecificDialog(
      const offline_items_collection::ContentId& content_id) override;
  IconState GetIconState() const override;

  // ToolbarButton:
  void UpdateIcon() override;
  void Layout(PassKey) override;
  bool ShouldShowInkdropAfterIphInteraction() override;

  // DownloadBubbleNavigationHandler:
  void OpenPrimaryDialog() override;
  void OpenSecurityDialog(
      const offline_items_collection::ContentId& content_id) override;
  void CloseDialog(views::Widget::ClosedReason reason) override;
  void OnSecurityDialogButtonPress(const DownloadUIModel& model,
                                   DownloadCommands::Command command) override;
  void OnDialogInteracted() override;
  std::unique_ptr<views::BubbleDialogDelegate::CloseOnDeactivatePin>
  PreventDialogCloseOnDeactivate() override;
  base::WeakPtr<DownloadBubbleNavigationHandler> GetWeakPtr() override;

  // BrowserListObserver
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserNoLongerActive(Browser* browser) override;

  // Deactivates the automatic closing of the partial bubble.
  void DeactivateAutoClose();

  DownloadBubbleUIController* bubble_controller() {
    return bubble_controller_.get();
  }

  DownloadDisplayController* display_controller() { return controller_.get(); }

  SkColor GetIconColor() const;

  void DisableAutoCloseTimerForTesting();
  void DisableDownloadStartedAnimationForTesting();

  DownloadBubbleContentsView* bubble_contents_for_testing() {
    return bubble_contents_;
  }

  void SetBubbleControllerForTesting(
      std::unique_ptr<DownloadBubbleUIController> bubble_controller);

 private:
  // Closes the bubble when it detects an event such as a mouse click, escape
  // key press, etc., which indicates the user's intent to close the bubble.
  // This is needed when the bubble is inactive (shown with ShowInactive)
  // because the normal close-on-deactivate mechanism doesn't work from an
  // already-inactive state. This is created by the DownloadToolbarButtonView
  // when the bubble is shown with ShowInactive, and is destroyed when the
  // bubble is closed.
  // TODO(crbug.com/40943500): Factor out common logic copied from translate
  // bubble.
  class BubbleCloser : public ui::EventObserver {
   public:
    explicit BubbleCloser(DownloadToolbarButtonView* toolbar_button);

    BubbleCloser(const BubbleCloser& other) = delete;
    BubbleCloser& operator=(const BubbleCloser& other) = delete;

    ~BubbleCloser() override;

    // ui::EventObserver:
    void OnEvent(const ui::Event& event) override;

   private:
    raw_ptr<DownloadToolbarButtonView> toolbar_button_ = nullptr;  // Owns this.
    std::unique_ptr<views::EventMonitor> event_monitor_;
  };

  // Max download count to show in the badge. Any higher number of downloads
  // results in a placeholder ("9+").
  static constexpr int kMaxDownloadCountDisplayed = 9;

  // views::Button overrides:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  ui::ImageModel GetBadgeImage(bool is_active,
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

  // Opens primary dialog and shows the item with given id, if found. Returns
  // pointer to the row if found, or nullptr if not found.
  DownloadBubbleRowView* ShowPrimaryDialogRow(
      std::optional<offline_items_collection::ContentId> content_id);

  // Callback invoked when the partial view is closed.
  void OnPartialViewClosed();

  // Helper function to show an IPH promo.
  void ShowIphPromo();

  // Called to automatically close the partial view, if such closing has not
  // been deactivated.
  void AutoClosePartialView();

  // Get the models for the primary view, which may be the full or the partial
  // view.
  std::vector<DownloadUIModel::DownloadUIModelPtr> GetPrimaryViewModels();

  // If |has_pending_download_started_animation_| is true, shows an animation of
  // a download icon moving upwards towards the toolbar icon.
  void ShowPendingDownloadStartedAnimation();

  bool ShouldShowBubbleAsInactive() const;

  void CloseAutofillPopup();

  // Whether to show the progress ring as a continuously spinning ring, during
  // deep scanning or if the progress is indeterminate.
  bool ShouldShowScanningAnimation() const;

  SkColor GetProgressColor(bool is_disabled, bool is_active) const;

  // Makes the required visual changes to set/unset the button into a dormant
  // or normal state.
  void UpdateIconDormant();

  // DownloadBubbleRowListViewInfoObserver implementation:
  void OnAnyRowRemoved() override;

  raw_ptr<Browser> browser_;
  bool is_primary_partial_view_ = false;
  // Controller for the DownloadToolbarButton UI.
  std::unique_ptr<DownloadDisplayController> controller_;
  // Controller for keeping track of items for both main view and partial view.
  std::unique_ptr<DownloadBubbleUIController> bubble_controller_;
  raw_ptr<views::BubbleDialogDelegate> bubble_delegate_ = nullptr;
  raw_ptr<DownloadBubbleContentsView> bubble_contents_ = nullptr;

  // Whether the progress ring in the icon should be updated continuously
  // (false), or the icon should be displayed as dormant (true). This is a
  // performance optimization to avoid redrawing the progress ring too many
  // times when a download is occurring. If the button is dormant, the progress
  // ring is drawn as a solid circle, and the icon color is the inactive color.
  // The badge is still drawn, and the icon shape is still updated. (These
  // change relatively infrequently, so it's ok to update them when they
  // change.) Buttons on browsers other than the most recent active browser for
  // the profile are generally dormant.
  bool is_dormant_ = false;

  // Following 3 fields are updated by the display controller and determine the
  // visual characteristics of the button icon. Note that they are still updated
  // as if the button is normal, even when the button is in a dormant state.

  // Current or pending state of the icon. If changing these, trigger
  // UpdateIcon() afterwards.
  IconState state_ = IconState::kComplete;
  IconActive active_ = IconActive::kInactive;

  // Parameters determining how the progress ring should be drawn.
  ProgressInfo progress_info_;

  // Whether we have a new progress_info_ and need to redraw the button.
  bool redraw_progress_soon_ = false;

  // Marks whether there is a pending download started animation. This is needed
  // because the animation should only be triggered after the view has been
  // laid out properly, so this provides a way to remember to show the animation
  // if needed, when performing layout.
  bool has_pending_download_started_animation_ = false;

// Overrides whether we are allowed to show the download started animation,
// may be false in tests.
#if BUILDFLAG(IS_CHROMEOS)
  // NOTE: Disabled on ChromeOS to respect its own download renderings.
  bool show_download_started_animation_ = false;
#else
  bool show_download_started_animation_ = true;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Tracks the task to automatically close the partial view after some amount
  // of time open, to minimize disruption to the user.
  base::RetainingOneShotTimer auto_close_bubble_timer_;
  // Whether the above timer does anything, which may be false in tests.
  bool use_auto_close_bubble_timer_ = true;

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

  gfx::SlideAnimation scanning_animation_{this};

  // Used for holding the top views visible while the download bubble is showing
  // in immersive mode on ChromeOS and Mac.
  std::unique_ptr<ImmersiveRevealedLock> immersive_revealed_lock_;

  std::unique_ptr<BubbleCloser> bubble_closer_;

  base::WeakPtrFactory<DownloadToolbarButtonView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_BUTTON_VIEW_H_
