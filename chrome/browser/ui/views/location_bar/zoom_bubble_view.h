// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_VIEW_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
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
                       public extensions::IconImage::Observer {
  METADATA_HEADER(ZoomBubbleView, LocationBarBubbleDelegateView)

 public:
  // Constructs ZoomBubbleView. Anchors the bubble to |anchor_view|, which must
  // not be nullptr. The bubble will auto-close when |reason| is AUTOMATIC. If
  // |immersive_mode_controller_| is present, the bubble will auto-close when
  // the top-of-window views are revealed.
  ZoomBubbleView(Browser* browser,
                 views::View* anchor_view,
                 content::WebContents* web_contents,
                 DisplayReason reason);
  ~ZoomBubbleView() override;

  ZoomBubbleView(const ZoomBubbleView&) = delete;
  ZoomBubbleView& operator=(const ZoomBubbleView&) = delete;

  // Refreshes the bubble by changing the zoom percentage appropriately and
  // resetting the timer if necessary.
  void Refresh();

  // Sets information about the extension that initiated the zoom change.
  // Calling this method asserts that the extension |extension| did initiate
  // the zoom change.
  void SetExtensionInfo(const extensions::Extension* extension);

  // Returns the ID of the extension that triggered the bubble, or an empty
  // string if it was not triggered by an extension.
  const std::string& extension_id() const { return extension_info_.id; }

  // Helpers for testing.
  std::u16string_view GetLabelForTesting() const;
  base::OneShotTimer* GetAutoCloseTimerForTesting();
  views::Button* GetResetButtonForTesting();
  views::Button* GetZoomInButtonForTesting();
  void OnKeyEventForTesting(ui::KeyEvent* event);

 private:
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

  // LocationBarBubbleDelegateView:
  std::u16string GetAccessibleWindowTitle() const override;
  void OnFocus() override;
  void OnBlur() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void Init() override;

  // extensions::IconImage::Observer
  void OnExtensionIconImageChanged(extensions::IconImage* /* image */) override;

  // Closes the bubble's widget.
  void Close();

  // Updates |label_| with the up to date zoom.
  void UpdateZoomPercent();

  // Starts a timer which will close the bubble if |auto_close_| is true.
  void StartTimerIfNecessary();

  // Stops the auto-close timer.
  void StopTimer();

  // Called when any button is pressed; does common logic, then runs |closure|.
  void ButtonPressed(base::RepeatingClosure closure);

  // Called by ButtonPressed() when |image_button_| is pressed.
  void ImageButtonPressed();

  raw_ptr<Browser> browser_;

  ZoomBubbleExtensionInfo extension_info_;

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
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_VIEW_H_
