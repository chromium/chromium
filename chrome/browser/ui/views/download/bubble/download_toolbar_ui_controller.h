// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_UI_CONTROLLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/download/download_bubble_row_list_view_info.h"
#include "chrome/browser/ui/download/download_display.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_navigation_handler.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

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
class DownloadBubbleUIController;

// DownloadToolbarUIController is a controller for the downloads button shown in
// the trusted area of the toolbar. This controller manages state, animations,
// and badges for the button. The icon is made visible when pinned or when
// downloads are in progress or when a download was initiated in the past 1
// hour.
class DownloadToolbarUIController
    : public DownloadDisplay,
      public DownloadBubbleNavigationHandler,
      public BrowserListObserver,
      public DownloadBubbleRowListViewInfoObserver {
 public:
  // Identifies the bubble dialog widget for testing.
  static constexpr char kBubbleName[] = "DownloadBubbleDialog";

  explicit DownloadToolbarUIController(BrowserView* browser_view);
  DownloadToolbarUIController(const DownloadToolbarUIController&) = delete;
  DownloadToolbarUIController& operator=(const DownloadToolbarUIController&) =
      delete;
  ~DownloadToolbarUIController() override;

  // Create the DownloadDisplayController, this must be called once the
  // PinnedToolbarActionsContainer is available since the
  // DownloadDisplayController can call Show() immediately.
  void Init();
  void TearDownPreBrowserWindowDestruction();

  // DownloadDisplay:
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
  IconState GetIconState() const override;

  void UpdateIcon();

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

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserNoLongerActive(Browser* browser) override;

  // Deactivates the automatic closing of the partial bubble.
  void DeactivateAutoClose();

  void InvokeUI();

  // If |has_pending_download_started_animation_| is true, shows an animation of
  // a download icon moving upwards towards the toolbar icon.
  void ShowPendingDownloadStartedAnimation();

  DownloadBubbleUIController* bubble_controller() {
    return bubble_controller_.get();
  }

  DownloadDisplayController* display_controller() { return controller_.get(); }

  DownloadBubbleContentsView* bubble_contents_for_testing() {
    return bubble_contents_;
  }

  base::RetainingOneShotTimer* auto_close_bubble_timer_for_testing() {
    return &auto_close_bubble_timer_;
  }

  bool IsProgressRingInDownloadingStateForTesting();
  bool IsProgressRingInDormantStateForTesting();
  views::ImageView* GetImageBadgeForTesting();

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
    explicit BubbleCloser(views::Button* toolbar_button,
                          base::WeakPtr<DownloadDisplay> download_display);

    BubbleCloser(const BubbleCloser& other) = delete;
    BubbleCloser& operator=(const BubbleCloser& other) = delete;

    ~BubbleCloser() override;

    // ui::EventObserver:
    void OnEvent(const ui::Event& event) override;

   private:
    base::WeakPtr<DownloadDisplay> download_display_;
    std::unique_ptr<views::EventMonitor> event_monitor_;
  };

  void CreateBubbleDialogDelegate();
  void OnBubbleClosing();

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

  bool ShouldShowBubbleAsInactive() const;

  void CloseAutofillPopup();

  // Whether to show the progress ring as a continuously spinning ring, during
  // deep scanning or if the progress is indeterminate.
  bool ShouldShowScanningAnimation() const;

  // Makes the required visual changes to set/unset the button into a dormant
  // or normal state.
  void UpdateIconDormant();

  // DownloadBubbleRowListViewInfoObserver:
  void OnAnyRowRemoved() override;

  raw_ptr<BrowserView> browser_view_;
  bool is_primary_partial_view_ = false;
  raw_ptr<actions::ActionItem> action_item_ = nullptr;
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

  // Following 2 fields are updated by the display controller and determine the
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
  bool show_download_started_animation_ = true;

  // Tracks the task to automatically close the partial view after some amount
  // of time open, to minimize disruption to the user.
  base::RetainingOneShotTimer auto_close_bubble_timer_;
  // Whether the above timer does anything, which may be false in tests.
  bool use_auto_close_bubble_timer_ = true;

  base::TimeTicks button_click_time_;

  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};

  // Maps number of in-progress downloads to the corresponding tooltip text, to
  // avoid having to create the strings repeatedly. The entry for 0 is the
  // default tooltip ("Downloads"), the entries for larger numbers are the
  // tooltips for N in-progress downloads ("N downloads in progress").
  std::map<int, std::u16string> tooltip_texts_;

  // Used for holding the top views visible while the download bubble is showing
  // in immersive mode on Mac.
  std::unique_ptr<ImmersiveRevealedLock> immersive_revealed_lock_;

  std::unique_ptr<BubbleCloser> bubble_closer_;

  base::WeakPtrFactory<DownloadToolbarUIController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_UI_CONTROLLER_H_
