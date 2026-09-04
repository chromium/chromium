// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_UI_UTIL_H_
#define CHROME_UPDATER_WIN_UI_UI_UTIL_H_

#include <windows.h>

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/win/scoped_gdi_object.h"

namespace updater::ui {

// Finds all the primary windows owned by the given process. A primary window is
// a top-level, has a system menu, and it is visible.
// Flags for window requirements.
inline constexpr uint32_t kWindowMustBeTopLevel = 0x00000001;
inline constexpr uint32_t kWindowMustHaveSysMenu = 0x00000002;
inline constexpr uint32_t kWindowMustBeVisible = 0x00000004;
bool FindProcessWindows(uint32_t process_id,
                        uint32_t window_flags,
                        std::vector<HWND>* windows);

// Forces the window to the foreground.
void MakeWindowForeground(HWND wnd);

// Returns true if the window is the main window of a process. That means that
// the window is visible and it is a top level window.
bool IsMainWindow(HWND wnd);

// Returns true if the window has a system menu.
bool HasSystemMenu(HWND wnd);

// System icon dimensions for big (ICON_BIG) and small (ICON_SMALL) icons.
struct IconSizes {
  int cx_big = 0;
  int cy_big = 0;
  int cx_small = 0;
  int cy_small = 0;
};

// Returns system icon dimensions scaled for `dpi` (or standard unscaled 96 DPI
// system metrics if `dpi` is 0).
IconSizes GetIconSizesForDpi(UINT dpi);

// Holds a pair of big (ICON_BIG) and small (ICON_SMALL) icon handles.
struct WindowIcons {
  base::win::ScopedGDIObject<HICON> icon_big;
  base::win::ScopedGDIObject<HICON> icon_small;
};

// Loads both big and small icons for `icon_resource_id` from the executable
// instance, scaled for `dpi` with symmetrical fallback to unscaled metrics if
// DPI-scaled loading fails.
WindowIcons LoadResourceIcons(int icon_resource_id, UINT dpi = 0);

// Dispatches WM_SETICON for both ICON_BIG and ICON_SMALL before replacing the
// handles in `current_icons`, ensuring the window never holds dangling
// references to destroyed handles.
void SetWindowIcons(HWND hwnd,
                    WindowIcons new_icons,
                    WindowIcons& current_icons);

// Creates an icon from an HBITMAP (such as an updater 24bpp 48x48 app logo BMP,
// a 32bpp ARGB icon bitmap, or a 32bpp compatible bitmap), scaled to the
// specified dimensions. If one dimension is 0, the specified dimension is used
// for both (square aspect). If both dimensions are omitted (width = 0, height =
// 0), dimensions default specifically to DPI-aware ICON_BIG system metrics for
// `dpi` (or 96 DPI system metrics if `dpi` is 0). Callers should pass explicit
// DPI-scaled metrics (e.g. from GetIconSizesForDpi) for small icons.
// Non-square source bitmaps (such as wide rectangular app logos) are fitted and
// centered within the target dimensions while preserving their aspect ratio;
// any unused letterbox margin is made transparent in the 1bpp mask.
// If the source bitmap is 32bpp with a true per-pixel alpha channel,
// synthesizes a 32bpp BITMAPV5HEADER icon with full alpha transparency. For
// opaque 24bpp or 32bpp bitmaps without an alpha channel, performs background
// color keying: samples the background color from (0, 0) (or tests for known
// light/dark dialog background colors RGB(255, 255, 255) and RGB(31, 31, 31),
// or uses `transparent_color` if provided) and keys out the connected
// background pixels in the 1bpp monochrome mask, setting transparent color
// pixels to RGB(0, 0, 0) to prevent Windows GDI XOR artifacts.
base::win::ScopedGDIObject<HICON> CreateIconFromHBitmap(
    HBITMAP bitmap,
    int width = 0,
    int height = 0,
    UINT dpi = 0,
    std::optional<COLORREF> transparent_color = std::nullopt);

// Returns a localized installer name for a bundle. If |bundle_name| is empty,
// the friendly company name is used.
std::wstring GetInstallerDisplayName(const std::u16string& bundle_name,
                                     const std::wstring& lang = {});

// Gets the text corresponding to a control in a dialog box.
bool GetDlgItemText(HWND dlg, int item_id, std::wstring* text);

// Returns true if the system is in high contrast mode.
bool IsHighContrastOn();

// Returns true if the system is in dark mode (or in high contrast mode with a
// dark theme).
bool IsDarkModeOn();

// Returns true if `color` has low perceived luminance (i.e. is dark).
bool IsColorDark(COLORREF color);

// Explicitly sets the arrow cursor if `wparam` matches `hwnd` (or a child
// control without its own class cursor) and `lparam` represents a client-area
// hit test (`HTCLIENT`), returning true if handled. Prevents Windows from
// sticking with the `IDC_APPSTARTING` ("Working in Background") cursor during
// GUI startup transitions.
bool MaybeSetArrowCursor(HWND hwnd, WPARAM wparam, LPARAM lparam);

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_UI_UTIL_H_
