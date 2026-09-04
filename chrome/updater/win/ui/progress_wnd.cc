// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/progress_wnd.h"

#include <windows.h>

#include <commctrl.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/version.h"
#include "base/win/current_module.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/scoped_select_object.h"
#include "chrome/updater/app/app_install_progress.h"
#include "chrome/updater/app/app_install_util_win.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "chrome/updater/win/ui/ui_constants.h"
#include "chrome/updater/win/ui/ui_ctls.h"
#include "chrome/updater/win/ui/ui_util.h"
#include "chrome/updater/win/ui/window_impl.h"

namespace updater::ui {

namespace {

// Returns true if all apps are cancelled or if the range is empty.
bool AreAllAppsCanceled(const std::vector<AppCompletionInfo>& apps_info) {
  return std::ranges::all_of(apps_info, [](const AppCompletionInfo& app_info) {
    return app_info.is_canceled;
  });
}

// Subclass procedure used for `SS_BITMAP` statics (`IDC_APP_BITMAP` and
// `IDC_ERROR_ILLUSTRATION`). Win32 does NOT send `WM_CTLCOLORSTATIC` for
// `SS_BITMAP` controls, so the dialog's dark-mode background brush cannot
// reach them through the usual mechanism. The defaults from the `STATIC`
// window class paint `COLOR_3DFACE` for both `WM_ERASEBKGND` and the
// no-image case in `WM_PAINT`, which shows up as an unwanted rectangle
// on themed / high-contrast backgrounds.
//
// In dark / high-contrast mode this subclass:
//   * Returns 1 from `WM_ERASEBKGND` so the parent's already-painted
//     themed background stays visible.
//   * If `WM_PAINT` arrives for a control that has no image set
//     (`STM_GETIMAGE` returns null), validates the paint rect without
//     drawing anything so the parent's themed background remains.
//
// When a bitmap IS set, `WM_PAINT` is forwarded to the default static
// proc so the bitmap is drawn normally. In light mode the entire
// default behavior (`COLOR_3DFACE` fill, then bitmap drawn on top) is
// preserved so the rainbow gradient design continues to look correct.
constexpr UINT_PTR kBitmapStaticSubclassId = 1;

LRESULT CALLBACK BitmapStaticSubclassProc(HWND hwnd,
                                          UINT msg,
                                          WPARAM wparam,
                                          LPARAM lparam,
                                          UINT_PTR id,
                                          DWORD_PTR ref_data) {
  if (msg == WM_THEMECHANGED || msg == WM_SYSCOLORCHANGE ||
      msg == WM_SETTINGCHANGE) {
    const bool themed_bg = IsHighContrastOn() || IsDarkModeOn();
    ::SetWindowSubclass(hwnd, BitmapStaticSubclassProc, id,
                        static_cast<DWORD_PTR>(themed_bg));
    return ::DefSubclassProc(hwnd, msg, wparam, lparam);
  }

  const bool themed_bg = static_cast<bool>(ref_data);
  if (msg == WM_ERASEBKGND && themed_bg) {
    return 1;
  }
  if (msg == WM_PAINT && themed_bg) {
    HBITMAP image = reinterpret_cast<HBITMAP>(
        ::SendMessageW(hwnd, STM_GETIMAGE, IMAGE_BITMAP, 0));
    if (!image) {
      // No image to draw. Validate the update region so Windows does not
      // re-issue `WM_PAINT`, and leave the parent's painted background
      // visible.
      PAINTSTRUCT ps = {};
      ::BeginPaint(hwnd, &ps);
      ::EndPaint(hwnd, &ps);
      return 0;
    }
  }
  if (msg == WM_NCDESTROY) {
    ::RemoveWindowSubclass(hwnd, BitmapStaticSubclassProc, id);
  }
  return ::DefSubclassProc(hwnd, msg, wparam, lparam);
}

void InstallBitmapStaticSubclass(HWND parent, int control_id) {
  HWND child = ::GetDlgItem(parent, control_id);
  if (child && ::IsWindow(child)) {
    const bool themed_bg = IsHighContrastOn() || IsDarkModeOn();
    ::SetWindowSubclass(child, BitmapStaticSubclassProc,
                        kBitmapStaticSubclassId,
                        static_cast<DWORD_PTR>(themed_bg));
  }
}

}  // namespace

ProgressWnd::ProgressWnd(MessageLoop* message_loop, HWND parent)
    : CompleteWnd(IDD_PROGRESS,
                  ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS,
                  message_loop,
                  parent,
                  base::UTF8ToWide(GetTagLanguage())) {}

ProgressWnd::~ProgressWnd() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsWindow()) {
    // TODO(crbug.com/447657543): replace with a check when the bug is fixed.
    base::debug::DumpWithoutCrashing();
  }
  cur_state_ = States::STATE_END;
}

void ProgressWnd::SetEventSink(ProgressWndEvents* events) {
  events_sink_ = events;
  CompleteWnd::SetEventSink(events_sink_);
}

LRESULT ProgressWnd::OnSetAppLogo(UINT, WPARAM wparam, LPARAM lparam) {
  // Extract the `HBITMAP` handles passed in `WPARAM` (light) and `LPARAM`
  // (dark).
  SetAppLogo(reinterpret_cast<HBITMAP>(wparam),
             reinterpret_cast<HBITMAP>(lparam));
  return 0;
}

RECT ProgressWnd::GetControlClientRect(HWND control) const {
  RECT rect = {};
  ::GetWindowRect(control, &rect);
  // Convert screen coordinates to parent client coordinates using an explicit
  // `POINT` array to avoid strict aliasing violations and support RTL layouts.
  POINT pts[] = {{rect.left, rect.top}, {rect.right, rect.bottom}};
  ::MapWindowPoints(HWND_DESKTOP, hwnd(), pts, 2);
  rect = {pts[0].x, pts[0].y, pts[1].x, pts[1].y};
  if (rect.left > rect.right) {
    std::swap(rect.left, rect.right);
  }
  return rect;
}

HBITMAP ProgressWnd::GetCurrentAppLogoBitmap() const {
  if (is_dark_mode()) {
    return dark_app_logo_bmp_.is_valid() ? dark_app_logo_bmp_.get()
                                         : light_app_logo_bmp_.get();
  }
  return light_app_logo_bmp_.is_valid() ? light_app_logo_bmp_.get()
                                        : dark_app_logo_bmp_.get();
}

void ProgressWnd::SetAppLogo(HBITMAP light_bitmap, HBITMAP dark_bitmap) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ResetWindowIconCache();
  if (light_app_logo_bmp_.get() != light_bitmap) {
    light_app_logo_bmp_.reset(light_bitmap);
  }
  if (dark_app_logo_bmp_.get() != dark_bitmap) {
    dark_app_logo_bmp_.reset(dark_bitmap);
  }
  UpdateAppLogo();
}

void ProgressWnd::UpdateAppLogo(UINT target_dpi) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }

  const UINT effective_dpi =
      target_dpi ? target_dpi : ::GetDpiForWindow(hwnd());

  const HWND app_bitmap_ctl = ::GetDlgItem(hwnd(), IDC_APP_BITMAP);
  if (!app_bitmap_ctl) {
    UpdateWindowIcon(nullptr, effective_dpi);
    return;
  }

  // Clears the logo image control and resets the window icon back to the
  // default application icon (IDI_APP).
  auto clear_logo = [this, app_bitmap_ctl, effective_dpi]() {
    const RECT ctl_rect = GetControlClientRect(app_bitmap_ctl);
    ::SendMessage(app_bitmap_ctl, STM_SETIMAGE, IMAGE_BITMAP, 0);
    scaled_app_logo_bmp_.reset();
    ::InvalidateRect(hwnd(), &ctl_rect, TRUE);
    UpdateWindowIcon(nullptr, effective_dpi);
  };

  HBITMAP current_logo = GetCurrentAppLogoBitmap();
  if (!current_logo) {
    clear_logo();
    return;
  }

  // Obtain the original dimensions of the cached bitmap.
  BITMAP bm = {};
  if (::GetObject(current_logo, sizeof(bm), &bm) == 0) {
    VLOG(1) << __func__ << " ::GetObject failed";
    clear_logo();
    return;
  }

  // Scale the bitmap dimensions based on the window's effective DPI.
  const int dpi_val = effective_dpi ? effective_dpi : USER_DEFAULT_SCREEN_DPI;
  const int width_pixels =
      ::MulDiv(bm.bmWidth, dpi_val, USER_DEFAULT_SCREEN_DPI);
  const int height_pixels =
      ::MulDiv(bm.bmHeight, dpi_val, USER_DEFAULT_SCREEN_DPI);

  if (width_pixels <= 0 || height_pixels <= 0) {
    VLOG(1) << __func__ << " Invalid logo dimensions: " << width_pixels << "x"
            << height_pixels;
    clear_logo();
    return;
  }

  HBITMAP scaled_bitmap = reinterpret_cast<HBITMAP>(
      ::CopyImage(current_logo, IMAGE_BITMAP, width_pixels, height_pixels, 0));
  if (!scaled_bitmap) {
    VLOG(1) << __func__ << " ::CopyImage failed to scale logo";
    clear_logo();
    return;
  }

  UpdateWindowIcon(current_logo, effective_dpi);

  RECT client_rect = {};
  ::GetClientRect(hwnd(), &client_rect);
  const int dialog_width = client_rect.right - client_rect.left;

  const RECT ctl_rect = GetControlClientRect(app_bitmap_ctl);

  // Lock the bottom edge of the control to the original design-time bottom
  // baseline. Taller square logos (e.g. 48x48) expand upward into the open
  // area below the progress bar without overlapping the dialog buttons below.
  if (initial_app_logo_base_bottom_y_ < 0) {
    initial_app_logo_base_bottom_y_ =
        ::MulDiv(ctl_rect.bottom, USER_DEFAULT_SCREEN_DPI, effective_dpi);
  }
  const int bottom_y = ::MulDiv(initial_app_logo_base_bottom_y_, effective_dpi,
                                USER_DEFAULT_SCREEN_DPI);
  const int x = (dialog_width - width_pixels) / 2;
  const int y = bottom_y - height_pixels;

  // Calculate the bounding box covering both the old and new control rects to
  // ensure complete repainting without leaving visual artifacts.
  const RECT old_rect = ctl_rect;
  const RECT new_rect = {x, y, x + width_pixels, bottom_y};
  RECT update_rect = {};
  ::UnionRect(&update_rect, &old_rect, &new_rect);

  ::SetWindowPos(app_bitmap_ctl, nullptr, x, y, width_pixels, height_pixels,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
  ::InvalidateRect(hwnd(), &update_rect, TRUE);

  // Pass the scaled `HBITMAP` handle as `LPARAM` to `STM_SETIMAGE`.
  ::SendMessage(app_bitmap_ctl, STM_SETIMAGE, IMAGE_BITMAP,
                reinterpret_cast<LPARAM>(scaled_bitmap));
  scaled_app_logo_bmp_.reset(scaled_bitmap);
}

LRESULT ProgressWnd::OnInitDialog(UINT, WPARAM, LPARAM) {
  HideWindowChildren(hwnd());

  InitializeDialog();

  const HWND app_bitmap_ctl = ::GetDlgItem(hwnd(), IDC_APP_BITMAP);
  if (app_bitmap_ctl) {
    const RECT ctl_rect = GetControlClientRect(app_bitmap_ctl);
    const int dpi = ::GetDpiForWindow(hwnd());
    const int effective_dpi = dpi ? dpi : USER_DEFAULT_SCREEN_DPI;
    // Record the design-time baseline bottom coordinate in 96-DPI space if not
    // already captured.
    if (initial_app_logo_base_bottom_y_ < 0) {
      initial_app_logo_base_bottom_y_ =
          ::MulDiv(ctl_rect.bottom, USER_DEFAULT_SCREEN_DPI, effective_dpi);
    }
  }

  SetMarqueeMode(true);

  SetControlText(IDC_INSTALLER_STATE_TEXT,
                 GetLocalizedString(IDS_INITIALIZING_BASE, lang()).c_str());

  // Suppress the default `WM_ERASEBKGND` handling for `SS_BITMAP` statics
  // so the dialog's themed background (dark / high contrast / rainbow)
  // shows through behind any bitmap content.
  InstallBitmapStaticSubclass(hwnd(), IDC_APP_BITMAP);
  InstallBitmapStaticSubclass(hwnd(), IDC_ERROR_ILLUSTRATION);

  btn1_.SetIsPrimary(true);
  btn1_.SubclassWindow(::GetDlgItem(hwnd(), IDC_BUTTON1));

  btn2_.SetIsPrimary(false);
  btn2_.SubclassWindow(::GetDlgItem(hwnd(), IDC_BUTTON2));

  close_btn_.SetIsPrimary(true);
  close_btn_.SubclassWindow(::GetDlgItem(hwnd(), IDC_CLOSE));

  get_help_btn_.SetIsPrimary(false);
  get_help_btn_.SubclassWindow(::GetDlgItem(hwnd(), IDC_GET_HELP));

  ChangeControlState();

  // Apply rounded corners on initialization.
  UpdateWindowRgn();

  // Force a full redraw of the dialog and all its children so the static
  // controls re-erase through the dark/gradient background painted by
  // `OnEraseBkgnd` instead of keeping their initial system-default
  // (BTNFACE) pixels.
  ::RedrawWindow(hwnd(), nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);

  return 1;  // Let the system set the focus.
}

LRESULT ProgressWnd::OnSize(UINT /*msg*/,
                            WPARAM /*wparam*/,
                            LPARAM /*lparam*/) {
  UpdateWindowRgn();
  SetMsgHandled(FALSE);  // Let other handlers process `WM_SIZE` if needed.
  return 0;
}

void ProgressWnd::UpdateWindowRgn() {
  // The window has a non-client area (e.g., system shadow, margins, or standard
  // borders). To ensure that the rounded window clipping region exactly aligns
  // with the custom border drawn in `OnEraseBkgnd` (which draws relative to the
  // client area bounds), we must calculate the client area coordinates relative
  // to the window coordinates. Using the window coordinates directly would
  // leave margins showing standard OS borders and un-clipped corners.
  RECT client_rect = {};
  ::GetClientRect(hwnd(), &client_rect);

  // Defensive check to prevent region creation with invalid or zero
  // coordinates.
  if (client_rect.right <= 0 || client_rect.bottom <= 0) {
    return;
  }

  // MapWindowPoints handles RTL window mirroring correctly by swapping
  // left/right coordinates when mapping a RECT to screen space (null dest HDC).
  ::MapWindowPoints(hwnd(), nullptr, reinterpret_cast<LPPOINT>(&client_rect),
                    2);

  RECT window_rect = {};
  ::GetWindowRect(hwnd(), &window_rect);

  const int left = client_rect.left - window_rect.left;
  const int top = client_rect.top - window_rect.top;
  const int right = client_rect.right - window_rect.left;
  const int bottom = client_rect.bottom - window_rect.top;

  // Scale the 11px corner radius based on the current DPI of the window to
  // ensure proportional rounded corners on high-DPI displays.
  const int scaled_radius = GetScaledCornerRadius();

  HRGN rgn = ::CreateRoundRectRgn(left, top, right, bottom, scaled_radius * 2,
                                  scaled_radius * 2);
  if (rgn) {
    // SetWindowRgn takes ownership of the HRGN object.
    ::SetWindowRgn(hwnd(), rgn, TRUE);
  }
}

int ProgressWnd::GetScaledCornerRadius() const {
  return ::MulDiv(11, ::GetDpiForWindow(hwnd()), USER_DEFAULT_SCREEN_DPI);
}

void ProgressWnd::ApplyDpiScaling(UINT dpi) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OmahaWnd::ApplyDpiScaling(dpi);
  UpdateAppLogo(dpi);
}

LRESULT ProgressWnd::OnEraseBkgnd(UINT, WPARAM wparam, LPARAM) {
  const HDC hdc = reinterpret_cast<HDC>(wparam);
  RECT rect = {};
  ::GetClientRect(hwnd(), &rect);

  const int width = rect.right - rect.left;
  const int height = rect.bottom - rect.top;
  if (width <= 0 || height <= 0) {
    return 1;
  }

  // Create an off-screen memory DC and bitmap (the primary buffer).
  base::win::ScopedCreateDC hdc_mem(::CreateCompatibleDC(hdc));
  if (!hdc_mem.is_valid()) {
    return 1;
  }
  base::win::ScopedGDIObject<HBITMAP> hbmp_mem(
      ::CreateCompatibleBitmap(hdc, width, height));
  if (!hbmp_mem.is_valid()) {
    return 1;
  }
  base::win::ScopedSelectObject select_mem_bmp(hdc_mem.get(), hbmp_mem.get());

  // Paint the background into the off-screen buffer.
  bool painted = false;

  // Background image is not loaded in High Contrast Mode.
  HBITMAP bg_bmp =
      is_high_contrast() ? nullptr : GetBackgroundBitmap(is_dark_mode());
  if (bg_bmp) {
    BITMAP bm = {};
    ::GetObject(bg_bmp, sizeof(bm), &bm);

    base::win::ScopedCreateDC hdc_src(::CreateCompatibleDC(hdc));
    if (hdc_src.is_valid()) {
      base::win::ScopedSelectObject select_src_bmp(hdc_src.get(), bg_bmp);

      // Set high-quality HALFTONE scaling mode on the memory DC.
      const int old_stretch_mode = ::SetStretchBltMode(hdc_mem.get(), HALFTONE);
      ::SetBrushOrgEx(hdc_mem.get(), 0, 0, nullptr);

      // Paint and stretch the background image over the off-screen client area.
      ::StretchBlt(hdc_mem.get(), 0, 0, width, height, hdc_src.get(), 0, 0,
                   bm.bmWidth, bm.bmHeight, SRCCOPY);

      // Restore DC state.
      ::SetStretchBltMode(hdc_mem.get(), old_stretch_mode);
      painted = true;
    }
  }

  if (!painted) {
    // Fallback to safe solid background color if loading fails.
    const COLORREF fallback_color =
        is_high_contrast() ? ::GetSysColor(COLOR_WINDOW)
                           : (is_dark_mode() ? kBgColorDark : kBgColorLight);
    base::win::ScopedGDIObject<HBRUSH> fill_brush(
        ::CreateSolidBrush(fallback_color));
    ::FillRect(hdc_mem.get(), &rect, fill_brush.get());
  }

  // Draw a 1px border at 30% opacity (or 100% system theme color in High
  // Contrast Mode) using RAII objects.
  const int scaled_radius = GetScaledCornerRadius();
  const int scaled_border = std::max(
      1, ::MulDiv(1, ::GetDpiForWindow(hwnd()), USER_DEFAULT_SCREEN_DPI));
  base::win::ScopedCreateDC hdc_blend(::CreateCompatibleDC(hdc));
  if (hdc_blend.is_valid()) {
    base::win::ScopedGDIObject<HBITMAP> hbmp_blend(
        ::CreateCompatibleBitmap(hdc, width, height));
    base::win::ScopedGDIObject<HRGN> border_rgn(::CreateRoundRectRgn(
        0, 0, width, height, scaled_radius * 2, scaled_radius * 2));

    const COLORREF border_color = is_high_contrast()
                                      ? ::GetSysColor(COLOR_WINDOWTEXT)
                                      : kWindowBorderColor;
    base::win::ScopedGDIObject<HBRUSH> border_brush(
        ::CreateSolidBrush(border_color));

    if (hbmp_blend.is_valid() && border_rgn.is_valid() &&
        border_brush.is_valid()) {
      base::win::ScopedSelectObject select_blend_bmp(hdc_blend.get(),
                                                     hbmp_blend.get());
      // Copy the background from hdc_mem to hdc_blend (NOT from display hdc)
      if (::BitBlt(hdc_blend.get(), 0, 0, width, height, hdc_mem.get(), 0, 0,
                   SRCCOPY)) {
        ::FrameRgn(hdc_blend.get(), border_rgn.get(), border_brush.get(),
                   scaled_border, scaled_border);

        BLENDFUNCTION bf = {
            .BlendOp = AC_SRC_OVER,
            .BlendFlags = 0,
            .SourceConstantAlpha =
                static_cast<BYTE>(is_high_contrast() ? 255 : 77),
            .AlphaFormat = 0,
        };

        // AlphaBlend hdc_blend back onto hdc_mem
        ::AlphaBlend(hdc_mem.get(), 0, 0, width, height, hdc_blend.get(), 0, 0,
                     width, height, bf);
      }
    }
  }

  // Blit the completed primary buffer (hdc_mem) to the screen window hdc.
  ::BitBlt(hdc, 0, 0, width, height, hdc_mem.get(), 0, 0, SRCCOPY);

  return 1;
}

HBITMAP ProgressWnd::GetBackgroundBitmap(bool is_dark_mode) {
  if (is_dark_mode) {
    if (!dark_bg_bmp_.is_valid()) {
      dark_bg_bmp_.reset(static_cast<HBITMAP>(
          ::LoadImage(CURRENT_MODULE(), MAKEINTRESOURCE(IDB_BACKGROUND_DARK),
                      IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION)));
    }
    return dark_bg_bmp_.get();
  } else {
    if (!light_bg_bmp_.is_valid()) {
      light_bg_bmp_.reset(static_cast<HBITMAP>(
          ::LoadImage(CURRENT_MODULE(), MAKEINTRESOURCE(IDB_BACKGROUND_LIGHT),
                      IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION)));
    }
    return light_bg_bmp_.get();
  }
}

HBITMAP ProgressWnd::GetErrorIllustrationBitmap(bool is_dark_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_dark_mode) {
    if (!dark_error_illustration_bmp_.is_valid()) {
      dark_error_illustration_bmp_.reset(static_cast<HBITMAP>(::LoadImage(
          CURRENT_MODULE(), MAKEINTRESOURCE(IDB_ERROR_ILLUSTRATION_DARK),
          IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION)));
    }
    return dark_error_illustration_bmp_.get();
  }

  if (!light_error_illustration_bmp_.is_valid()) {
    light_error_illustration_bmp_.reset(static_cast<HBITMAP>(
        ::LoadImage(CURRENT_MODULE(), MAKEINTRESOURCE(IDB_ERROR_ILLUSTRATION),
                    IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION)));
  }
  return light_error_illustration_bmp_.get();
}

void ProgressWnd::UpdateErrorIllustration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }

  const HWND error_ctl = ::GetDlgItem(hwnd(), IDC_ERROR_ILLUSTRATION);
  if (!error_ctl || !(::GetWindowLongPtr(error_ctl, GWL_STYLE) & WS_VISIBLE)) {
    return;
  }

  HBITMAP current_illustration = GetErrorIllustrationBitmap(is_dark_mode());
  if (!current_illustration) {
    return;
  }

  if (reinterpret_cast<HBITMAP>(::SendMessage(
          error_ctl, STM_GETIMAGE, IMAGE_BITMAP, 0)) == current_illustration) {
    return;
  }

  ::SendMessage(error_ctl, STM_SETIMAGE, IMAGE_BITMAP,
                reinterpret_cast<LPARAM>(current_illustration));
  const RECT ctl_rect = GetControlClientRect(error_ctl);
  ::InvalidateRect(hwnd(), &ctl_rect, TRUE);
  ::InvalidateRect(error_ctl, nullptr, TRUE);
}

void ProgressWnd::ResetThemeResources() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Refresh theme state early so `UpdateAppLogo()` and
  // `UpdateErrorIllustration()` (via `is_dark_mode()`) evaluate the new theme
  // before parent and descendant layouts repaint in `OmahaWnd`. Calling
  // `UpdateThemeState()` here is safe and idempotent, even though `OmahaWnd`
  // will invoke it again when the message bubbles up.
  UpdateThemeState();
  light_bg_bmp_.reset();
  dark_bg_bmp_.reset();
  if (HWND error_ctl = ::GetDlgItem(hwnd(), IDC_ERROR_ILLUSTRATION)) {
    ::SendMessage(error_ctl, STM_SETIMAGE, IMAGE_BITMAP, 0);
  }
  light_error_illustration_bmp_.reset();
  dark_error_illustration_bmp_.reset();
  UpdateAppLogo();
  UpdateErrorIllustration();
}

LRESULT ProgressWnd::OnSettingChange(UINT, WPARAM, LPARAM lparam) {
  SetMsgHandled(FALSE);
  if (!lparam || std::wstring_view(reinterpret_cast<LPCWSTR>(lparam)) ==
                     L"ImmersiveColorSet") {
    ResetThemeResources();
  }
  return 0;
}

LRESULT ProgressWnd::OnThemeChanged(UINT, WPARAM, LPARAM) {
  SetMsgHandled(FALSE);
  ResetThemeResources();
  return 0;
}

HBRUSH ProgressWnd::OnCtlColorStatic(HDC dc, HWND) {
  if (is_high_contrast()) {
    ::SetTextColor(dc, ::GetSysColor(COLOR_WINDOWTEXT));
    ::SetBkColor(dc, ::GetSysColor(COLOR_WINDOW));
  } else if (is_dark_mode()) {
    ::SetTextColor(dc, kTextColorDark);
  }
  ::SetBkMode(dc, TRANSPARENT);
  return static_cast<HBRUSH>(::GetStockObject(NULL_BRUSH));
}

void ProgressWnd::SetControlText(int id, const std::wstring& text) {
  const HWND hwnd_control = ::GetDlgItem(hwnd(), id);
  if (!hwnd_control || !::IsWindow(hwnd_control)) {
    return;
  }

  // Reduces flicker by only updating the control if the text has changed.
  std::wstring current_text;
  ui::GetDlgItemText(hwnd(), id, &current_text);
  if (text == current_text) {
    return;
  }

  // Get the control's rectangle relative to the dialog.
  RECT rect = {};
  ::GetWindowRect(hwnd_control, &rect);
  POINT top_left = {rect.left, rect.top};
  POINT bottom_right = {rect.right, rect.bottom};
  ::ScreenToClient(hwnd(), &top_left);
  ::ScreenToClient(hwnd(), &bottom_right);
  rect = {top_left.x, top_left.y, bottom_right.x, bottom_right.y};

  // Invalidate the area on the parent. This forces the parent to redraw the
  // gradient in this specific spot.
  ::InvalidateRect(hwnd(), &rect, TRUE);

  // Update the text.
  ::SetWindowTextW(hwnd_control, text.c_str());
}

// If closing is disabled, then it does not close the window.
// If in a completion state, then the window is closed.
// Otherwise, `HandleCancelRequest` is called which attempts to cancel the
// install.
bool ProgressWnd::MaybeCloseWindow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_close_enabled()) {
    return false;
  }

  if (cur_state_ != States::STATE_COMPLETE_SUCCESS &&
      cur_state_ != States::STATE_COMPLETE_ERROR &&
      cur_state_ != States::STATE_COMPLETE_RESTART_BROWSER &&
      cur_state_ != States::STATE_COMPLETE_RESTART_ALL_BROWSERS &&
      cur_state_ != States::STATE_COMPLETE_REBOOT) {
    // The UI is not in final state: attempt to cancel the install.
    HandleCancelRequest();
    return false;
  }

  CloseWindow();
  return true;
}

void ProgressWnd::OnClickedButton(UINT notify_code, int id, HWND wnd_ctl) {
  CHECK(id == IDC_BUTTON1 || id == IDC_BUTTON2 || id == IDC_CLOSE);
  CHECK(events_sink_);

  switch (id) {
    case IDC_BUTTON1:
      switch (cur_state_) {
        case States::STATE_COMPLETE_RESTART_BROWSER:
          events_sink_->DoRestartBrowser(false, post_install_urls_);
          break;
        case States::STATE_COMPLETE_RESTART_ALL_BROWSERS:
          events_sink_->DoRestartBrowser(true, post_install_urls_);
          break;
        case States::STATE_COMPLETE_REBOOT:
          events_sink_->DoReboot();
          break;
        default:
          NOTREACHED();
      }
      break;
    case IDC_BUTTON2:
      switch (cur_state_) {
        case States::STATE_COMPLETE_RESTART_BROWSER:
        case States::STATE_COMPLETE_RESTART_ALL_BROWSERS:
        case States::STATE_COMPLETE_REBOOT:
          break;
        default:
          NOTREACHED();
      }
      break;
    case IDC_CLOSE:
      switch (cur_state_) {
        case States::STATE_CHECKING_FOR_UPDATE:
        case States::STATE_WAITING_TO_DOWNLOAD:
        case States::STATE_DOWNLOADING:
        case States::STATE_WAITING_TO_INSTALL:
        case States::STATE_INSTALLING:
        case States::STATE_PAUSED:
        case States::STATE_COMPLETE_SUCCESS:
        case States::STATE_COMPLETE_ERROR:
          CompleteWnd::OnClickedButton(notify_code, id, wnd_ctl);
          return;
        default:
          NOTREACHED();
      }
  }

  CloseWindow();
}

void ProgressWnd::HandleCancelRequest() {
  SetControlText(IDC_INSTALLER_STATE_TEXT,
                 GetLocalizedString(IDS_CANCELING_BASE, lang()).c_str());

  if (is_canceled_) {
    return;
  }
  is_canceled_ = true;
  if (events_sink_) {
    events_sink_->DoCancel();
  }
}

void ProgressWnd::OnCheckingForUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }

  cur_state_ = States::STATE_CHECKING_FOR_UPDATE;

  SetControlText(
      IDC_INSTALLER_STATE_TEXT,
      GetLocalizedString(IDS_WAITING_TO_CONNECT_BASE, lang()).c_str());

  ChangeControlState();
}

void ProgressWnd::OnUpdateAvailable(const std::string& app_id,
                                    const std::u16string& app_name,
                                    const base::Version& version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ProgressWnd::OnWaitingToDownload(const std::string& app_id,
                                      const std::u16string& app_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }
  cur_state_ = States::STATE_WAITING_TO_DOWNLOAD;
  SetMarqueeMode(true);
  SetControlText(IDC_INSTALLER_STATE_TEXT, L"");
  ChangeControlState();
}

// May be called repeatedly during download.
void ProgressWnd::OnDownloading(
    const std::string& app_id,
    const std::u16string& app_name,
    const std::optional<base::TimeDelta> time_remaining,
    int pos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }

  CHECK(0 <= pos && pos <= 100);

  if (States::STATE_DOWNLOADING != cur_state_) {
    cur_state_ = States::STATE_DOWNLOADING;
    ChangeControlState();
  }

  std::wstring s;

  if (is_canceled_) {
    s = GetLocalizedString(IDS_CANCELING_BASE, lang());
  } else if (time_remaining && time_remaining->InSeconds() == 0) {
    s = GetLocalizedString(IDS_DOWNLOADING_COMPLETED_BASE, lang());
  } else {
    s = GetLocalizedString(IDS_DOWNLOADING_BASE, lang());
  }

  SetControlText(IDC_INSTALLER_STATE_TEXT, s.c_str());

  SetMarqueeMode(pos == 0);
  if (pos > 0) {
    ::SendDlgItemMessageW(hwnd(), IDC_PROGRESS, PBM_SETPOS, pos, 0);
  }
}

void ProgressWnd::OnWaitingRetryDownload(const std::string& app_id,
                                         const std::u16string& app_name,
                                         base::Time next_retry_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }

  cur_state_ = States::STATE_WAITING_TO_DOWNLOAD;
  SetMarqueeMode(true);
  SetControlText(IDC_INSTALLER_STATE_TEXT, L"");
  ChangeControlState();
}

void ProgressWnd::OnWaitingToInstall(const std::string& app_id,
                                     const std::u16string& app_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }

  if (States::STATE_WAITING_TO_INSTALL != cur_state_) {
    cur_state_ = States::STATE_WAITING_TO_INSTALL;
    SetMarqueeMode(true);
    SetControlText(
        IDC_INSTALLER_STATE_TEXT,
        GetLocalizedString(IDS_WAITING_TO_INSTALL_BASE, lang()).c_str());
    ChangeControlState();
  }
}

// May be called repeatedly during install.
void ProgressWnd::OnInstalling(
    const std::string& app_id,
    const std::u16string& app_name,
    const std::optional<base::TimeDelta> time_remaining,
    int pos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }

  if (States::STATE_INSTALLING != cur_state_) {
    cur_state_ = States::STATE_INSTALLING;
    SetControlText(IDC_INSTALLER_STATE_TEXT,
                   GetLocalizedString(IDS_INSTALLING_BASE, lang()).c_str());
    ChangeControlState();
  }

  SetMarqueeMode(pos <= 0);
  if (pos > 0) {
    ::SendDlgItemMessageW(hwnd(), IDC_PROGRESS, PBM_SETPOS, pos, 0);
  }
}

void ProgressWnd::OnPause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }

  cur_state_ = States::STATE_PAUSED;
  ChangeControlState();
}

void ProgressWnd::DeterminePostInstallUrls(const ObserverCompletionInfo& info) {
  CHECK(post_install_urls_.empty());
  post_install_urls_.clear();

  for (const AppCompletionInfo& app_info : info.apps_info) {
    if (!app_info.post_install_url.is_empty() &&
        (app_info.completion_code ==
             CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS ||
         app_info.completion_code ==
             CompletionCodes::COMPLETION_CODE_RESTART_BROWSER)) {
      post_install_urls_.push_back(app_info.post_install_url);
    }
  }
  CHECK(!post_install_urls_.empty());
}

CompletionCodes ProgressWnd::GetBundleCompletionCode(
    const ObserverCompletionInfo& info) {
  if (info.completion_code == CompletionCodes::COMPLETION_CODE_ERROR ||
      info.completion_code ==
          CompletionCodes::COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL) {
    return info.completion_code;
  }

  CHECK_EQ(info.completion_code, CompletionCodes::COMPLETION_CODE_SUCCESS);

  return info.apps_info.empty()
             ? kCompletionCodesActionPriority[0]
             : std::ranges::max_element(
                   info.apps_info,
                   [](const auto& app_info1, const auto& app_info2) {
                     return GetPriority(app_info1.completion_code) <
                            GetPriority(app_info2.completion_code);
                   })
                   ->completion_code;
}

std::wstring ProgressWnd::GetBundleCompletionErrorMessages(
    const ObserverCompletionInfo& info) {
  // Combine non-empty app installation completion messages. App-specific
  // installation error message usually gives more details than the generic one.
  std::vector<std::u16string> completion_texts;
  for (const AppCompletionInfo& app_info : info.apps_info) {
    if (!app_info.completion_message.empty()) {
      completion_texts.push_back(app_info.completion_message);
    }
  }

  // Or fallback to the default bundle failure message if nothing is available.
  if (completion_texts.empty() && !info.completion_text.empty()) {
    completion_texts.push_back(info.completion_text);
  }

  return base::UTF16ToWide(base::JoinString(completion_texts, u"\n"));
}

void ProgressWnd::OnComplete(const ObserverCompletionInfo& observer_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CompleteWnd::OnComplete()) {
    return;
  }

  bool launch_commands_succeeded = LaunchCmdLines(observer_info);

  CompletionCodes overall_completion_code =
      GetBundleCompletionCode(observer_info);
  switch (overall_completion_code) {
    case CompletionCodes::COMPLETION_CODE_SUCCESS:
    case CompletionCodes::COMPLETION_CODE_LAUNCH_COMMAND:
    case CompletionCodes::COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      CompleteWnd::DisplayCompletionDialog(
          true, base::UTF16ToWide(observer_info.completion_text),
          observer_info.help_url.possibly_invalid_spec());
      break;
    case CompletionCodes::COMPLETION_CODE_ERROR:
      if (AreAllAppsCanceled(observer_info.apps_info)) {
        CloseWindow();
        return;
      }
      cur_state_ = States::STATE_COMPLETE_ERROR;
      CompleteWnd::DisplayCompletionDialog(
          false, GetBundleCompletionErrorMessages(observer_info),
          observer_info.help_url.possibly_invalid_spec());
      break;
    case CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS:
      cur_state_ = States::STATE_COMPLETE_RESTART_ALL_BROWSERS;
      SetControlText(IDC_BUTTON1,
                     GetLocalizedString(IDS_RESTART_NOW_BASE, lang()).c_str());
      SetControlText(
          IDC_BUTTON2,
          GetLocalizedString(IDS_RESTART_LATER_BASE, lang()).c_str());
      SetControlText(
          IDC_COMPLETE_TEXT,
          GetLocalizedStringF(IDS_TEXT_RESTART_ALL_BROWSERS_BASE,
                              base::UTF16ToWide(bundle_name()), lang())
              .c_str());
      DeterminePostInstallUrls(observer_info);
      break;
    case CompletionCodes::COMPLETION_CODE_RESTART_BROWSER:
      cur_state_ = States::STATE_COMPLETE_RESTART_BROWSER;
      SetControlText(IDC_BUTTON1,
                     GetLocalizedString(IDS_RESTART_NOW_BASE, lang()).c_str());
      SetControlText(
          IDC_BUTTON2,
          GetLocalizedString(IDS_RESTART_LATER_BASE, lang()).c_str());
      SetControlText(
          IDC_COMPLETE_TEXT,
          GetLocalizedStringF(IDS_TEXT_RESTART_BROWSER_BASE,
                              base::UTF16ToWide(bundle_name()), lang())
              .c_str());
      DeterminePostInstallUrls(observer_info);
      break;
    case CompletionCodes::COMPLETION_CODE_REBOOT:
      cur_state_ = States::STATE_COMPLETE_REBOOT;
      SetControlText(IDC_BUTTON1,
                     GetLocalizedString(IDS_RESTART_NOW_BASE, lang()).c_str());
      SetControlText(
          IDC_BUTTON2,
          GetLocalizedString(IDS_RESTART_LATER_BASE, lang()).c_str());
      SetControlText(
          IDC_COMPLETE_TEXT,
          GetLocalizedStringF(IDS_TEXT_RESTART_COMPUTER_BASE,
                              base::UTF16ToWide(bundle_name()), lang())
              .c_str());
      break;
    case CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      CompleteWnd::DisplayCompletionDialog(
          true,
          GetLocalizedStringF(IDS_TEXT_RESTART_ALL_BROWSERS_BASE,
                              base::UTF16ToWide(bundle_name()), lang()),
          observer_info.help_url.possibly_invalid_spec());
      break;
    case CompletionCodes::COMPLETION_CODE_REBOOT_NOTICE_ONLY:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      CompleteWnd::DisplayCompletionDialog(
          true,
          GetLocalizedStringF(IDS_TEXT_RESTART_COMPUTER_BASE,
                              base::UTF16ToWide(bundle_name()), lang()),
          observer_info.help_url.possibly_invalid_spec());
      break;
    case CompletionCodes::COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      CompleteWnd::DisplayCompletionDialog(
          true,
          GetLocalizedStringF(IDS_TEXT_RESTART_BROWSER_BASE,
                              base::UTF16ToWide(bundle_name()), lang()),
          observer_info.help_url.possibly_invalid_spec());
      break;
    case CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      if (launch_commands_succeeded) {
        CloseWindow();
        return;
      }
      CompleteWnd::DisplayCompletionDialog(
          true, base::UTF16ToWide(observer_info.completion_text),
          observer_info.help_url.possibly_invalid_spec());
      break;
    case CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY:
      cur_state_ = States::STATE_COMPLETE_SUCCESS;
      CloseWindow();
      return;
  }

  ChangeControlState();
}

HRESULT ProgressWnd::ChangeControlState() {
  for (const ControlState& ctl : ctls_) {
    const size_t i = std::to_underlying(cur_state_);
    CHECK_LE(i, std::size(ctl.attr));
    SetControlAttributes(ctl.id, ctl.attr[i]);
  }
  return S_OK;
}

HRESULT ProgressWnd::SetMarqueeMode(bool is_marquee) {
  if (is_marquee == is_marquee_) {
    return S_OK;
  }

  HWND progress_bar = ::GetDlgItem(hwnd(), IDC_PROGRESS);
  if (!progress_bar) {
    return E_FAIL;
  }

  is_marquee_ = is_marquee;

  LONG_PTR style = ::GetWindowLongPtrW(progress_bar, GWL_STYLE);
  if (is_marquee) {
    style |= PBS_MARQUEE;
  } else {
    style &= ~PBS_MARQUEE;
  }
  ::SetWindowLongPtrW(progress_bar, GWL_STYLE, style);
  ::SendMessageW(progress_bar, PBM_SETMARQUEE, is_marquee,
                 kMarqueeModeUpdatesMs);

  return S_OK;
}

}  // namespace updater::ui
