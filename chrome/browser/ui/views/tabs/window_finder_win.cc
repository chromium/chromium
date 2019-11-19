// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/window_finder.h"

#include <objbase.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include "base/macros.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/windows_version.h"
#include "ui/aura/window.h"
#include "ui/display/win/screen_win.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"
#include "ui/views/win/hwnd_util.h"

namespace {

// BaseWindowFinder -----------------------------------------------------------

// Base class used to locate a window. This is intended to be used with the
// various win32 functions that iterate over windows.
//
// A subclass need only override ShouldStopIterating to determine when
// iteration should stop.
class BaseWindowFinder {
 public:
  // Creates a BaseWindowFinder with the specified set of HWNDs to ignore.
  explicit BaseWindowFinder(const std::set<HWND>& ignore) : ignore_(ignore) {}
  virtual ~BaseWindowFinder() {}

 protected:
  static BOOL CALLBACK WindowCallbackProc(HWND hwnd, LPARAM lParam) {
    // Cast must match that in as_lparam().
    BaseWindowFinder* finder = reinterpret_cast<BaseWindowFinder*>(lParam);
    if (finder->ignore_.find(hwnd) != finder->ignore_.end())
      return TRUE;

    return finder->ShouldStopIterating(hwnd) ? FALSE : TRUE;
  }

  LPARAM as_lparam() {
    // Cast must match that in WindowCallbackProc().
    return reinterpret_cast<LPARAM>(static_cast<BaseWindowFinder*>(this));
  }

  // Returns true if iteration should stop, false if iteration should continue.
  virtual bool ShouldStopIterating(HWND window) = 0;

 private:
  const std::set<HWND>& ignore_;

  DISALLOW_COPY_AND_ASSIGN(BaseWindowFinder);
};

// TopMostFinder --------------------------------------------------------------

// Helper class to determine if a particular point of a window is not obscured
// by another window.
class TopMostFinder : public BaseWindowFinder {
 public:
  // Returns true if |window| is the topmost window at the location
  // |screen_loc|, not including the windows in |ignore|.
  static bool IsTopMostWindowAtPoint(HWND window,
                                     const gfx::Point& screen_loc,
                                     const std::set<HWND>& ignore) {
    TopMostFinder finder(window, screen_loc, ignore);
    return finder.is_top_most_;
  }

  bool ShouldStopIterating(HWND hwnd) override {
    if (hwnd == target_) {
      // Window is topmost, stop iterating.
      is_top_most_ = true;
      return true;
    }

    if (!IsWindowVisible(hwnd)) {
      // The window isn't visible, keep iterating.
      return false;
    }

    RECT r;
    if (!GetWindowRect(hwnd, &r) || !PtInRect(&r, screen_loc_.ToPOINT())) {
      // The window doesn't contain the point, keep iterating.
      return false;
    }

    LONG ex_styles = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (ex_styles & WS_EX_TRANSPARENT || ex_styles & WS_EX_LAYERED) {
      // Mouse events fall through WS_EX_TRANSPARENT windows, so we ignore them.
      //
      // WS_EX_LAYERED is trickier. Apps like Switcher create a totally
      // transparent WS_EX_LAYERED window that is always on top. If we don't
      // ignore WS_EX_LAYERED windows and there are totally transparent
      // WS_EX_LAYERED windows then there are effectively holes on the screen
      // that the user can't reattach tabs to. So we ignore them. This is a bit
      // problematic in so far as WS_EX_LAYERED windows need not be totally
      // transparent in which case we treat chrome windows as not being obscured
      // when they really are, but this is better than not being able to
      // reattach tabs.
      return false;
    }

    // hwnd is at the point. Make sure the point is within the windows region.
    if (GetWindowRgn(hwnd, tmp_region_.get()) == ERROR) {
      // There's no region on the window and the window contains the point. Stop
      // iterating.
      return true;
    }

    // The region is relative to the window's rect.
    BOOL is_point_in_region = PtInRegion(
        tmp_region_.get(), screen_loc_.x() - r.left, screen_loc_.y() - r.top);
    tmp_region_.reset(CreateRectRgn(0, 0, 0, 0));
    // Stop iterating if the region contains the point.
    return !!is_point_in_region;
  }

 private:
  TopMostFinder(HWND window,
                const gfx::Point& screen_loc,
                const std::set<HWND>& ignore)
      : BaseWindowFinder(ignore),
        target_(window),
        is_top_most_(false),
        tmp_region_(CreateRectRgn(0, 0, 0, 0)) {
    screen_loc_ = display::win::ScreenWin::DIPToScreenPoint(screen_loc);
    EnumWindows(WindowCallbackProc, as_lparam());
  }

  // The window we're looking for.
  HWND target_;

  // Location of window to find in pixel coordinates.
  gfx::Point screen_loc_;

  // Is target_ the top most window? This is initially false but set to true
  // in ShouldStopIterating if target_ is passed in.
  bool is_top_most_;

  base::win::ScopedRegion tmp_region_;

  DISALLOW_COPY_AND_ASSIGN(TopMostFinder);
};

// LocalProcessWindowFinder ---------------------------------------------------

// Helper class to determine if a particular point contains a window from our
// process.
class LocalProcessWindowFinder : public BaseWindowFinder {
 public:
  // Returns the hwnd from our process at screen_loc that is not obscured by
  // another window. Returns NULL otherwise.
  static gfx::NativeWindow GetProcessWindowAtPoint(
      const gfx::Point& screen_loc,
      const std::set<HWND>& ignore) {
    LocalProcessWindowFinder finder(screen_loc, ignore);
    // Windows 8 has a window that appears first in the list of iterated
    // windows, yet is not visually on top of everything.
    // TODO(sky): figure out a better way to ignore this window.
    if (finder.result_ && ((base::win::OSInfo::GetInstance()->version() >=
                            base::win::Version::WIN8) ||
                           TopMostFinder::IsTopMostWindowAtPoint(
                               finder.result_, screen_loc, ignore))) {
      return views::DesktopWindowTreeHostWin::GetContentWindowForHWND(
          finder.result_);
    }
    return NULL;
  }

 protected:
  bool ShouldStopIterating(HWND hwnd) override {
    RECT r;

    // Make sure the window is on the same virtual desktop.
    if (virtual_desktop_manager_) {
      BOOL on_current_desktop;
      if (SUCCEEDED(virtual_desktop_manager_->IsWindowOnCurrentVirtualDesktop(
              hwnd, &on_current_desktop)) &&
          !on_current_desktop) {
        return false;
      }
    }

    if (IsWindowVisible(hwnd) && GetWindowRect(hwnd, &r) &&
        PtInRect(&r, screen_loc_.ToPOINT())) {
      result_ = hwnd;
      return true;
    }
    return false;
  }

 private:
  LocalProcessWindowFinder(const gfx::Point& screen_loc,
                           const std::set<HWND>& ignore)
      : BaseWindowFinder(ignore),
        result_(NULL) {
    if (base::win::GetVersion() >= base::win::Version::WIN10) {
      ::CoCreateInstance(__uuidof(VirtualDesktopManager), nullptr, CLSCTX_ALL,
                         IID_PPV_ARGS(&virtual_desktop_manager_));
    }
    screen_loc_ = display::win::ScreenWin::DIPToScreenPoint(screen_loc);
    EnumThreadWindows(GetCurrentThreadId(), WindowCallbackProc, as_lparam());
  }

  // Position of the mouse in pixel coordinates.
  gfx::Point screen_loc_;

  // The resulting window. This is initially null but set to true in
  // ShouldStopIterating if an appropriate window is found.
  HWND result_;

  // Only used on Win10+.
  Microsoft::WRL::ComPtr<IVirtualDesktopManager> virtual_desktop_manager_;

  DISALLOW_COPY_AND_ASSIGN(LocalProcessWindowFinder);
};

std::set<HWND> RemapIgnoreSet(const std::set<gfx::NativeView>& ignore) {
  std::set<HWND> hwnd_set;
  std::set<gfx::NativeView>::const_iterator it = ignore.begin();
  for (; it != ignore.end(); ++it) {
    HWND w = (*it)->GetHost()->GetAcceleratedWidget();
    if (w)
      hwnd_set.insert(w);
  }
  return hwnd_set;
}

}  // namespace

gfx::NativeWindow WindowFinder::GetLocalProcessWindowAtPoint(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore) {
  return LocalProcessWindowFinder::GetProcessWindowAtPoint(
      screen_point, RemapIgnoreSet(ignore));
}
