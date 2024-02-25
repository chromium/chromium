// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_icon_image.h"
#include "ui/views/controls/label.h"

class Browser;

namespace content {
class WebContents;
}

namespace views {
class Button;
class ImageButton;
}  // namespace views

// View used to display the zoom percentage when it has changed.
class ZoomBubbleView : public LocationBarBubbleDelegateView,
                       public ImmersiveModeController::Observer,
                       public extensions::IconImage::Observer {
  METADATA_HEADER(ZoomBubbleView, LocationBarBubbleDelegateView)

 public:
  ZoomBubbleView(const ZoomBubbleView&) = delete;
  ZoomBubbleView& operator=(const ZoomBubbleView&) = delete;

  // Shows the bubble and automatically closes it after a short time period if
  // |reason| is AUTOMATIC.
  static void ShowBubble(content::WebContents* web_contents,
                         DisplayReason reason);

  // If the bubble is being shown for the given |web_contents|, refreshes it.
  static bool RefreshBubbleIfShowing(const content::WebContents* web_contents);

  // Closes the showing bubble (if one exists).
  static void CloseCurrentBubble();

  // Returns the zoom bubble if the zoom bubble is showing. Returns NULL
  // otherwise.
  static ZoomBubbleView* GetZoomBubble();

  // Refreshes the bubble by changing the zoom percentage appropriately and
  // resetting the timer if necessary.
  void Refresh();

 private:
  FRIEND_TEST_ALL_PREFIXES(ZoomBubbleBrowserTest, ImmersiveFullscreen);
  FRIEND_TEST_ALL_PREFIXES(ZoomBubbleBrowserTest,
                           BubbleSuppressingExtensionRefreshesExistingBubble);
  FRIEND_TEST_ALL_PREFIXES(ZoomBubbleBrowserTest, FocusPreventsClose);
  FRIEND_TEST_ALL_PREFIXES(ZoomBubbleImmersiveDisabledBrowserTest,
                           AnchorPositionsInFullscreen);

  // Returns true if we can reuse the existing bubble for the given
  // |web_contents|.
  static bool CanRefresh(const content::WebContents* web_contents);

  // Stores information about the extension that initiated the zoom change, if
  // any.
  struct ZoomBubbleExtensionInfo {
    ZoomBubbleExtensionInfo();
    ~ZoomBubbleExtensionInfo();

    // The unique id of the extension, which is used to find the correct
    // extension after clicking on the image button in the zoom bubble.
    std::string id;

    // The name of the extension, which appears in the tooltip of the image
    // button in the zoom bubble.
    std::string name;

    // An image of the extension's icon, which appears in the zoom bubble as an
    // image button.
    std::unique_ptr<const extensions::IconImage> icon_image;
  };

  // Constructs ZoomBubbleView. Anchors the bubble to |anchor_view|, which must
  // not be nullptr. The bubble will auto-close when |reason| is AUTOMATIC. If
  // |immersive_mode_controller_| is present, the bubble will auto-close when
  // the top-of-window views are revealed.
  ZoomBubbleView(views::View* anchor_view,
                 content::WebContents* web_contents,
                 DisplayReason reason,
                 ImmersiveModeController* immersive_mode_controller);
  ~ZoomBubbleView() override;

  // LocationBarBubbleDelegateView:
  std::u16string GetAccessibleWindowTitle() const override;
  void OnFocus() override;
  void OnBlur() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void Init() override;
  void WindowClosing() override;
  void CloseBubble() override;

  // ImmersiveModeController::Observer
  void OnImmersiveRevealStarted() override;
  void OnImmersiveModeControllerDestroyed() override;

  // extensions::IconImage::Observer
  void OnExtensionIconImageChanged(extensions::IconImage* /* image */) override;

  // Sets information about the extension that initiated the zoom change.
  // Calling this method asserts that the extension |extension| did initiate
  // the zoom change.
  void SetExtensionInfo(const extensions::Extension* extension);

  // Updates |label_| with the up to date zoom.
  void UpdateZoomPercent();

  // Updates visibility of the zoom icon.
  void UpdateZoomIconVisibility();

  // Starts a timer which will close the bubble if |auto_close_| is true.
  void StartTimerIfNecessary();

  // Stops the auto-close timer.
  void StopTimer();

  // Called when any button is pressed; does common logic, then runs |closure|.
  void ButtonPressed(base::RepeatingClosure closure);

  // Called by ButtonPressed() when |image_button_| is pressed.
  void ImageButtonPressed();

  // Gets the browser for `web_contents()`. May return null.
  Browser* GetBrowser() const;

  ZoomBubbleExtensionInfo extension_info_;

  // Singleton instance of the zoom bubble. The zoom bubble can only be shown on
  // the active browser window, so there is no case in which it will be shown
  // twice at the same time.
  static ZoomBubbleView* zoom_bubble_;

  // Timer used to auto close the bubble.
  base::OneShotTimer auto_close_timer_;

  // Timer duration that is made longer if a user presses + or - buttons.
  base::TimeDelta auto_close_duration_;

  // Image button in the zoom bubble that will show the |extension_icon_| image
  // if an extension initiated the zoom change, and links to that extension at
  // "chrome://extensions".
  raw_ptr<views::ImageButton> image_button_ = nullptr;

  // Label displaying the zoom percentage.
  raw_ptr<views::Label> label_ = nullptr;

  // Action buttons that can change zoom.
  raw_ptr<views::Button> zoom_out_button_ = nullptr;
  raw_ptr<views::Button> zoom_in_button_ = nullptr;
  raw_ptr<views::Button> reset_button_ = nullptr;

  // Whether the currently displayed bubble will automatically close.
  bool auto_close_;

  // Used to ignore close requests generated automatically in response to
  // button presses, since pressing a button in the bubble should not trigger
  // closing.
  bool ignore_close_bubble_ = false;

  // The immersive mode controller for the BrowserView containing
  // |web_contents_|.
  // Not owned.
  raw_ptr<ImmersiveModeController> immersive_mode_controller_;

  // The session of the Browser that triggered the bubble. This allows the zoom
  // icon to be updated even if the WebContents is destroyed.
  const SessionID session_id_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_VIEW_H_
