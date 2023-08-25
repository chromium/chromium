// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_BUBBLE_VIEWS_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/status_bubble.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}
namespace gfx {
class Animation;
class Point;
}
namespace views {
class View;
class Widget;
}

// StatusBubble displays a bubble of text that fades in, hovers over the
// browser chrome and fades away when not needed. It is primarily designed
// to allow users to see where hovered links point to.
class StatusBubbleViews : public StatusBubble {
 public:
  class StatusView;

  // How wide the bubble's shadow is.
  static const int kShadowThickness;

  // The combined vertical padding above and below the text.
  static const int kTotalVerticalPadding = 7;

  // |base_view| is the view that this bubble is positioned relative to.
  explicit StatusBubbleViews(views::View* base_view);

  StatusBubbleViews(const StatusBubbleViews&) = delete;
  StatusBubbleViews& operator=(const StatusBubbleViews&) = delete;

  ~StatusBubbleViews() override;

  views::View* base_view() { return base_view_; }

  // Reposition the bubble's popup - as we are using a WS_POPUP for the bubble,
  // we have to manually position it when the browser window moves.
  void RepositionPopup();

  // The bubble only has a preferred height: the sum of the height of
  // the font and kTotalVerticalPadding.
  int GetPreferredHeight();

  // Calculate and set new position for status bubble.
  void Reposition();

  // Set bubble to new width.
  void SetBubbleWidth(int width);

  // Gets the width that a bubble should be for a given string
  int GetWidthForURL(const std::u16string& url_string);

  // Overridden from StatusBubble:
  void SetStatus(const std::u16string& status) override;
  void SetURL(const GURL& url) override;
  void Hide() override;
  void MouseMoved(bool left_content) override;
  void UpdateDownloadShelfVisibility(bool visible) override;

 protected:
  views::Widget* popup() { return popup_.get(); }

  // Notify a mouse event with current mouse location. The location is (0,0)
  // when the mouse is at the top-left of the screen.
  void MouseMovedAt(const gfx::Point& location, bool left_content);

 private:
  class StatusViewAnimation;
  class StatusViewExpander;

  friend class StatusBubbleViewsTest;
  friend class StatusView;

  // Initializes the popup and view.
  void InitPopup();

  // Destroys the popup and view.
  void DestroyPopup();

  // Attempt to move the status bubble out of the way of the cursor, allowing
  // users to see links in the region normally occupied by the status bubble.
  void AvoidMouse(const gfx::Point& location);

  // Returns true if the base_view_'s widget is visible and not minimized.
  bool IsFrameVisible();

  // Returns true if the base_view_'s widget is maximized.
  bool IsFrameMaximized();

  // Expand bubble size to accommodate a long URL.
  void ExpandBubble();

  // Cancel all waiting expansion animations in the timer.
  void CancelExpandTimer();

  // Get the standard width for a status bubble in the current frame size.
  int GetStandardStatusBubbleWidth();

  // Get the maximum possible width for a status bubble in the current frame
  // size.
  int GetMaxStatusBubbleWidth();

  // Set the bounds of the bubble relative to |base_view_|.
  void SetBounds(int x, int y, int w, int h);

  gfx::Animation* GetShowHideAnimationForTest();
  bool IsDestroyPopupTimerRunningForTest();

  // The status text we want to display when there are no URLs to display.
  std::u16string status_text_;

  // The url we want to display when there is no status text to display.
  std::u16string url_text_;

  // The original, non-elided URL.
  GURL url_;

  // Position relative to the base_view_.
  gfx::Point original_position_;
  // original_position_ adjusted according to the current RTL.
  gfx::Point position_;
  gfx::Size size_;

  // Last location passed to MouseMoved().
  gfx::Point last_mouse_moved_location_;

  // Whether the view contains the mouse.
  bool contains_mouse_ = false;

  // How vertically offset the bubble is from its root position_.
  int offset_ = 0;

  // Use a Widget for the popup so that it floats above all content as well as
  // going outside the bounds of the hosting widget.
  std::unique_ptr<views::Widget> popup_;

  raw_ptr<views::View, AcrossTasksDanglingUntriaged> base_view_;
  raw_ptr<StatusView, DanglingUntriaged> view_ = nullptr;

  // Manages the expansion of a status bubble to fit a long URL.
  std::unique_ptr<StatusViewExpander> expand_view_;

  // If the download shelf is visible, do not obscure it.
  bool download_shelf_is_visible_ = false;

  // If the bubble has already been expanded, and encounters a new URL,
  // change size immediately, with no hover.
  bool is_expanded_ = false;

  // Used for posting tasks. This is typically
  // base::SingleThreadTaskRunner::GetCurrentDefault(), but may be set to
  // something else for tests.
  raw_ptr<base::SequencedTaskRunner> task_runner_;

  // Times expansion of status bubble when URL is too long for standard width.
  base::WeakPtrFactory<StatusBubbleViews> expand_timer_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_BUBBLE_VIEWS_H_
