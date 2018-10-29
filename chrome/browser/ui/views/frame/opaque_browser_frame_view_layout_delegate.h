// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_LAYOUT_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_LAYOUT_DELEGATE_H_

namespace gfx {
class Size;
}

// Delegate interface to control layout decisions without having to depend on
// Browser{,Frame,View}.
class OpaqueBrowserFrameViewLayoutDelegate {
 public:
  // Controls the visual placement of the window icon/title in non-tabstrip
  // mode.
  virtual bool ShouldShowWindowIcon() const = 0;
  virtual bool ShouldShowWindowTitle() const = 0;
  virtual base::string16 GetWindowTitle() const = 0;

  // Returns the size of the window icon. This can be platform dependent
  // because of differences in fonts, so its part of the interface.
  virtual int GetIconSize() const = 0;

  // Returns the browser's minimum view size. Used because we need to calculate
  // the minimum size for the entire non-client area.
  virtual gfx::Size GetBrowserViewMinimumSize() const = 0;

  // Whether we should show the (minimize,maximize,close) buttons. This can
  // depend on the current state of the window (e.g., whether it is maximized).
  virtual bool ShouldShowCaptionButtons() const = 0;

  // Returns true if in guest mode or a non off the record session.
  virtual bool IsRegularOrGuestSession() const = 0;

  // Controls window state.
  virtual bool IsMaximized() const = 0;
  virtual bool IsMinimized() const = 0;

  virtual bool IsTabStripVisible() const = 0;
  virtual int GetTabStripHeight() const = 0;
  virtual bool IsToolbarVisible() const = 0;

  // Returns the tabstrips preferred size so the frame layout can work around
  // it.
  virtual gfx::Size GetTabstripPreferredSize() const = 0;

  // Computes the height of the top area of the frame.
  virtual int GetTopAreaHeight() const = 0;

  // Returns true if the window frame is rendered by Chrome.
  virtual bool UseCustomFrame() const = 0;

  // Determines whether the top frame is condensed vertically, as when the
  // window is maximized. If true, the top frame is just the height of a tab,
  // rather than having extra vertical space above the tabs. This also removes
  // the thick frame border and rounded corners.
  virtual bool IsFrameCondensed() const = 0;

  // Returns whether the shapes of background tabs are visible against the frame
  // for either active or inactive windows.
  virtual bool EverHasVisibleBackgroundTabShapes() const = 0;

 protected:
  virtual ~OpaqueBrowserFrameViewLayoutDelegate() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_LAYOUT_DELEGATE_H_
