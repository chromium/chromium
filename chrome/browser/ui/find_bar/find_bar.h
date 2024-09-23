// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is an interface for the platform specific FindBar.  It is responsible
// for drawing the FindBar bar on the platform and is owned by the
// FindBarController.

#ifndef CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_H_
#define CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_H_

#include <string>

#include "build/build_config.h"

class FindBarController;
class FindBarTesting;

namespace find_in_page {
class FindNotificationDetails;
}

namespace gfx {
class Point;
class Range;
}

#if BUILDFLAG(IS_MAC)
namespace views {
class Widget;
}
#endif

class FindBar {
 public:
  virtual ~FindBar() { }

  // Accessor and setter for the FindBarController.
  virtual FindBarController* GetFindBarController() const = 0;
  virtual void SetFindBarController(
      FindBarController* find_bar_controller) = 0;

  // Shows the find bar. Any previous search string will again be visible.
  // If |animate| is true, we try to slide the find bar in.
  virtual void Show(bool animate) = 0;

  // Hide the find bar.  If |animate| is true, we try to slide the find bar
  // away.
  virtual void Hide(bool animate) = 0;

  // Restore the selected text in the find box and focus it.
  virtual void SetFocusAndSelection() = 0;

  // Clear the text in the find box.
  virtual void ClearResults(
      const find_in_page::FindNotificationDetails& results) = 0;

  // Stop the animation.
  virtual void StopAnimation() = 0;

  // Repaints and lays out the find bar window relative to the view layout state
  // of the current browser window.
  virtual void MoveWindowIfNecessary() = 0;

  // Set the text in the find box.
  virtual void SetFindTextAndSelectedRange(
      const std::u16string& find_text,
      const gfx::Range& selected_range) = 0;

  // Gets the search string currently visible in the find box.
  virtual std::u16string GetFindText() const = 0;

  // Gets the selection.
  virtual gfx::Range GetSelectedRange() const = 0;

  // Updates the FindBar with the find result details contained within the
  // specified |result|.
  virtual void UpdateUIForFindResult(
      const find_in_page::FindNotificationDetails& result,
      const std::u16string& find_text) = 0;

  // No match was found; play an audible alert.
  virtual void AudibleAlert() = 0;

  virtual bool IsFindBarVisible() const = 0;

  // Upon dismissing the window, restore focus to the last focused view which is
  // not FindBarView or any of its children.
  virtual void RestoreSavedFocus() = 0;

  // Returns true if all tabs use a single find pasteboard.
  virtual bool HasGlobalFindPasteboard() const = 0;

  // Called when the web contents associated with the find bar changes.
  virtual void UpdateFindBarForChangedWebContents() = 0;

  // Returns a pointer to the testing interface to the FindBar, or NULL
  // if there is none.
  virtual const FindBarTesting* GetFindBarTesting() const = 0;

#if BUILDFLAG(IS_MAC)
  // Get the host widget. Used by immersive fullscreen to detect the find bar
  // widget and reparent as necessary.
  virtual views::Widget* GetHostWidget() = 0;
#endif
};

class FindBarTesting {
 public:
  virtual ~FindBarTesting() { }

  // Computes the location of the find bar and whether it is fully visible in
  // its parent window. The return value indicates if the window is visible at
  // all. Both out arguments are optional.
  //
  // This is used for UI tests of the find bar. If the find bar is not currently
  // shown (return value of false), the out params will be {(0, 0), false}.
  virtual bool GetFindBarWindowInfo(gfx::Point* position,
                                    bool* fully_visible) const = 0;

  // Gets the search string currently selected in the Find box.
  virtual std::u16string GetFindSelectedText() const = 0;

  // Gets the match count text (ie. 1 of 3) visible in the Find box.
  virtual std::u16string GetMatchCountText() const = 0;

  // Gets the pixel width of the FindBar contents.
  virtual int GetContentsWidth() const = 0;

  // Gets the number of audible alerts that have been issued by this bar.
  virtual size_t GetAudibleAlertCount() const = 0;
};

#endif  // CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_H_
