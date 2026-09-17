// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/ui.h"

#include <windows.h>

#include <uxtheme.h>

#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/win/current_module.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/ui/ui_constants.h"
#include "chrome/updater/win/ui/ui_util.h"

namespace updater::ui {

namespace {

// Creates a font given a point size in tenths of a point at the system DPI.
// Mirrors WTL's `CFont::CreatePointFont` helper.
HFONT CreatePointFontW(int point_size_tenths,
                       LPCWSTR face_name,
                       int weight = FW_NORMAL) {
  HDC screen_dc = ::GetDC(nullptr);
  const int logical_pixels_y = ::GetDeviceCaps(screen_dc, LOGPIXELSY);
  ::ReleaseDC(nullptr, screen_dc);
  // Height in logical pixels: MulDiv(point_size_tenths, dpi, 720)
  // (720 = 72 points/inch * 10 tenths).
  const int height = ::MulDiv(point_size_tenths, logical_pixels_y, 720);
  return ::CreateFontW(-height, 0, 0, 0, weight, FALSE, FALSE, 0,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                       face_name);
}

void SetItemFont(HWND parent, int item_id, HFONT font) {
  HWND ctl = ::GetDlgItem(parent, item_id);
  if (ctl) {
    ::SendMessageW(ctl, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  }
}

}  // namespace

const OmahaWnd::ControlAttributes OmahaWnd::kVisibleTextAttributes = {
    false, true, true, false, false};
const OmahaWnd::ControlAttributes OmahaWnd::kDefaultActiveButtonAttributes = {
    false, true, true, true, true};
const OmahaWnd::ControlAttributes OmahaWnd::kDisabledButtonAttributes = {
    false, false, false, true, false};
const OmahaWnd::ControlAttributes OmahaWnd::kNonDefaultActiveButtonAttributes =
    {false, true, true, true, false};
const OmahaWnd::ControlAttributes OmahaWnd::kVisibleImageAttributes = {
    false, true, false, false, false};
const OmahaWnd::ControlAttributes OmahaWnd::kDisabledNonButtonAttributes = {
    false, false, false, false, false};

void EnableFlatButtons(HWND hwnd_parent) {
  ::EnumChildWindows(
      hwnd_parent,
      [](HWND hwnd, LPARAM) {
        CHECK(hwnd);
        const DWORD style =
            static_cast<DWORD>(::GetWindowLongW(hwnd, GWL_STYLE));
        if (style & BS_FLAT) {
          ::SetWindowTheme(hwnd, L"", L"");
        }
        return TRUE;
      },
      0);
}

void HideWindowChildren(HWND hwnd_parent) {
  ::EnumChildWindows(
      hwnd_parent,
      [](HWND hwnd, LPARAM) {
        CHECK(hwnd);
        ::ShowWindow(hwnd, SW_HIDE);
        return TRUE;
      },
      0);
}

OmahaWnd::OmahaWnd(int dialog_id,
                   MessageLoop* message_loop,
                   HWND parent,
                   const std::wstring& lang)
    : IDD(dialog_id),
      message_loop_(message_loop),
      parent_(parent),
      lang_(lang),
      is_complete_(false),
      is_close_enabled_(true),
      events_sink_(nullptr),
      scope_(UpdaterScope::kUser) {
  CHECK(message_loop);
}

OmahaWnd::~OmahaWnd() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!IsWindow());
}

HRESULT OmahaWnd::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!Create(IDD, parent_)) {
    VLOG(1) << "Failed to create the window";
    return E_FAIL;
  }
  message_loop_->AddMessageFilter(this);

  return S_OK;
}

BOOL OmahaWnd::PreTranslateMessage(MSG* msg) {
  return ::IsDialogMessageW(hwnd(), msg);
}

void OmahaWnd::InitializeDialog() {
  ::SetWindowTextW(hwnd(),
                   GetInstallerDisplayName(bundle_name(), lang()).c_str());

  CenterWindow(hwnd(), nullptr);
  UpdateWindowIcon(nullptr, nullptr);

  // Disable the maximize system menu item.
  HMENU menu = ::GetSystemMenu(hwnd(), FALSE);
  VLOG_IF(2, !menu) << "Failed to find system menu";
  if (menu) {
    ::EnableMenuItem(menu, SC_MAXIMIZE, MF_BYCOMMAND | MF_GRAYED);
  }

  progress_bar_.SubclassWindow(::GetDlgItem(hwnd(), IDC_PROGRESS));

  default_font_.reset(CreatePointFontW(100, kDialogFont, FW_NORMAL));
  SendMessageToDescendants(hwnd(), WM_SETFONT,
                           reinterpret_cast<WPARAM>(default_font_.get()), 0);

  header_font_.reset(CreatePointFontW(180, kDialogFont, FW_MEDIUM));
  SetItemFont(hwnd(), IDC_INSTALLER_STATE_TEXT, header_font_.get());

  font_.reset(CreatePointFontW(160, kDialogFont, FW_NORMAL));
  SetItemFont(hwnd(), IDC_INFO_TEXT, font_.get());
  SetItemFont(hwnd(), IDC_COMPLETE_TEXT, font_.get());
  SetItemFont(hwnd(), IDC_ERROR_TEXT, font_.get());

  CreateOwnerDrawTitleBar(hwnd(), ::GetDlgItem(hwnd(), IDC_TITLE_BAR_SPACER),
                          kBkColor);
  SetCustomDlgColors(kTextColor, kBkColor);

  EnableFlatButtons(hwnd());
}

void OmahaWnd::ResetWindowIconCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Clears cached logo, DPI, and badge state to prevent false cache hits if
  // Windows GDI reallocates a new bitmap at the same handle address. Note that
  // window_icons_ handles are intentionally not destroyed here so the window
  // never holds dangling icon references before UpdateWindowIcon() dispatches
  // new handles.
  current_logo_big_ = nullptr;
  current_logo_small_ = nullptr;
  current_dpi_ = 0;
  current_badge_resource_id_ = std::nullopt;
}

void OmahaWnd::UpdateWindowIcon(HBITMAP big_bitmap,
                                HBITMAP small_bitmap,
                                UINT dpi,
                                std::optional<int> badge_resource_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow()) {
    return;
  }
  // Prefer the explicit DPI passed from ApplyDpiScaling during WM_DPICHANGED,
  // as GetDpiForWindow() may not yet reflect the updated DPI before window
  // bounds adjustment completes.
  const UINT target_dpi = dpi ? dpi : ::GetDpiForWindow(hwnd());
  if (big_bitmap == current_logo_big_ && small_bitmap == current_logo_small_ &&
      target_dpi == current_dpi_ &&
      badge_resource_id == current_badge_resource_id_ &&
      window_icons_.icon_big.is_valid() &&
      window_icons_.icon_small.is_valid()) {
    return;
  }

  // In hybrid theme mode, `big_bitmap` and `small_bitmap` may differ (e.g.
  // `big_bitmap` from `light_app_logo_bmp_` for a light taskbar via ICON_BIG,
  // and `small_bitmap` from `dark_app_logo_bmp_` for a dark titlebar via
  // ICON_SMALL). If either bitmap is null, fall back to the other available
  // bitmap so both sizes are populated.
  const HBITMAP effective_big_bitmap = big_bitmap ? big_bitmap : small_bitmap;
  const HBITMAP effective_small_bitmap =
      small_bitmap ? small_bitmap : big_bitmap;

  WindowIcons icons;
  bool badge_applied = false;
  if (effective_big_bitmap) {
    const IconSizes sizes = GetIconSizesForDpi(target_dpi);
    base::win::ScopedGDIObject<HICON> badge_icon_big;

    // Loading takes < 0.1 ms and happens very infrequently, therefore loading
    // on demand is a simple and effective implementation.
    // Overlay badges are applied only to `ICON_BIG` (taskbar and Alt-Tab).
    // Small titlebar icons (e.g. 16x16) omit the badge overlay because an 8x8
    // downscaled badge destroys logo legibility.
    if (badge_resource_id.has_value()) {
      const RECT badge_rect = GetBadgeRect(sizes.cx_big, sizes.cy_big);
      const int badge_width = badge_rect.right - badge_rect.left;
      const int badge_height = badge_rect.bottom - badge_rect.top;
      badge_icon_big =
          base::win::ScopedGDIObject<HICON>(reinterpret_cast<HICON>(
              ::LoadImage(CURRENT_MODULE(),
                          MAKEINTRESOURCE(*badge_resource_id), IMAGE_ICON,
                          badge_width, badge_height, LR_DEFAULTCOLOR)));
      if (!badge_icon_big.is_valid()) {
        VLOG(1) << __func__ << ": Failed to load badge icon resource "
                << *badge_resource_id << "; proceeding unbadged";
      }
    }

    icons.icon_big = CreateIconFromHBitmap(
        effective_big_bitmap, sizes.cx_big, sizes.cy_big, target_dpi,
        /*transparent_color=*/std::nullopt, badge_icon_big.get(),
        &badge_applied);
    icons.icon_small = CreateIconFromHBitmap(
        effective_small_bitmap, sizes.cx_small, sizes.cy_small, target_dpi,
        /*transparent_color=*/std::nullopt, /*badge_icon=*/nullptr);
  }

  // Tradeoff (Clean Unbadged Fallback vs Pre-Download Badge Compositing):
  // When `effective_big_bitmap` is null (prior to the application logo download
  // completing), we do not attempt to composite the installer badge onto the
  // fallback `IDI_APP` icon. Attempting to overlay a badge onto the generic app
  // icon produces distorted, double-scaled icons before the brand logo arrives.
  // Instead, both big and small window icons consistently fall back to the
  // clean, unbadged default application icon (`IDI_APP`), preventing mixed or
  // visually corrupted states during initial setup.
  if (!icons.icon_big.is_valid() || !icons.icon_small.is_valid()) {
    icons = LoadResourceIcons(IDI_APP, target_dpi);
    // Fallback occurred: do not cache bitmaps as active so future attempts can
    // retry generating icons once the brand logo arrives.
    current_logo_big_ = nullptr;
    current_logo_small_ = nullptr;
    current_badge_resource_id_ = std::nullopt;
  } else {
    current_logo_big_ = big_bitmap;
    current_logo_small_ = small_bitmap;
    current_badge_resource_id_ =
        badge_applied ? badge_resource_id : std::nullopt;
  }
  current_dpi_ = target_dpi;

  // Dispatches WM_SETICON with the new icon handles and moves ownership into
  // `window_icons_` via reference, safely replacing the old handles without
  // dangling references.
  SetWindowIcons(hwnd(), std::move(icons), window_icons_);
}

LRESULT OmahaWnd::OnClose(UINT, WPARAM, LPARAM) {
  MaybeCloseWindow();
  return 0;
}

HRESULT OmahaWnd::CloseWindow() {
  HRESULT hr = DestroyWindow() ? S_OK : HRESULTFromLastError();
  if (events_sink_) {
    events_sink_->DoClose();
  }
  return hr;
}

void OmahaWnd::MaybeRequestExitProcess() {
  if (!is_complete_) {
    return;
  }

  RequestExitProcess();
}

void OmahaWnd::RequestExitProcess() {
  if (events_sink_) {
    events_sink_->DoExit();
  }
}

LRESULT OmahaWnd::OnNCDestroy(UINT, WPARAM, LPARAM) {
  message_loop_->RemoveMessageFilter(this);
  MaybeRequestExitProcess();
  SetMsgHandled(FALSE);  // Let default processing handle the WM_NCDESTROY.
  return 0;
}

LRESULT OmahaWnd::OnDpiChanged(UINT, WPARAM wparam, LPARAM lparam) {
  ApplySuggestedWindowRect(hwnd(), lparam);

  // Re-render text/graphics for the new DPI.
  ApplyDpiScaling(/*new_dpi=*/LOWORD(wparam));

  // Resize the title bar.
  RecalcLayout(hwnd(), ::GetDlgItem(hwnd(), IDC_TITLE_BAR_SPACER));

  // Force a full redraw of everything.
  ::RedrawWindow(hwnd(), nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
  return 0;
}

void OmahaWnd::RepaintForThemeChange(bool notify_descendants,
                                     UINT msg,
                                     WPARAM wparam,
                                     LPARAM lparam) {
  // Subclasses reload before anything paints, so `is_dark_mode()`-derived
  // bitmaps are current when the parent and descendant layouts repaint.
  OnThemeStateChanged();
  if (notify_descendants) {
    SendMessageToDescendants(hwnd(), msg, wparam, lparam);
  }
  ::RedrawWindow(hwnd(), nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

LRESULT OmahaWnd::OnSettingChange(UINT msg, WPARAM wparam, LPARAM) {
  // Ignore setting changes that are not theme-related or produced no state
  // change.
  if (!CouldBeThemeSettingChange(wparam) || !UpdateThemeState()) {
    return 0;
  }

  RepaintForThemeChange(/*notify_descendants=*/true, msg, wparam,
                        /*lparam=*/0);
  return 0;
}

LRESULT OmahaWnd::OnThemeChanged(UINT msg, WPARAM wparam, LPARAM lparam) {
  SetMsgHandled(FALSE);
  // Unconditionally reload theme resources on WM_THEMECHANGED /
  // WM_SYSCOLORCHANGE.
  UpdateThemeState();
  // Windows delivers WM_THEMECHANGED to every child itself.
  RepaintForThemeChange(/*notify_descendants=*/msg != WM_THEMECHANGED, msg,
                        wparam, lparam);
  return 0;
}

LRESULT OmahaWnd::OnSetCursor(UINT, WPARAM wparam, LPARAM lparam) {
  if (MaybeSetArrowCursor(hwnd(), wparam, lparam)) {
    return TRUE;
  }
  SetMsgHandled(FALSE);
  return 0;
}

// Called when ESC key is pressed.
void OmahaWnd::OnCancel(UINT, int id, HWND) {
  CHECK_EQ(id, IDCANCEL);

  if (!is_close_enabled_) {
    return;
  }

  MaybeCloseWindow();
}

void OmahaWnd::Show() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWindow() || ::IsWindowVisible(hwnd())) {
    return;
  }

  CenterWindow(hwnd(), nullptr);
  SetVisible(true);

  if (!::SetForegroundWindow(hwnd())) {
    PLOG(WARNING) << __func__ << ": ::SetForegroundWindow failed";
  }
}

void OmahaWnd::ApplyDpiScaling(UINT dpi) {
  const UINT effective_dpi =
      dpi ? dpi
          : (IsWindow() ? ::GetDpiForWindow(hwnd()) : USER_DEFAULT_SCREEN_DPI);
  // Calculate new font height: (DesiredPointSize * dpi) / 72. Use a negative
  // number for height to request the character height in CreateFontW.
  const int dpi_val = static_cast<int>(effective_dpi);
  const int font_height = ::MulDiv(10, dpi_val, 72);

  default_font_.reset(::CreateFontW(
      -font_height,                 // nHeight
      0,                            // nWidth
      0,                            // nEscapement
      0,                            // nOrientation
      FW_NORMAL,                    // nWeight
      FALSE,                        // bItalic
      FALSE,                        // bUnderline
      0,                            // cStrikeOut
      DEFAULT_CHARSET,              // nCharSet
      OUT_DEFAULT_PRECIS,           // nOutPrecision
      CLIP_DEFAULT_PRECIS,          // nClipPrecision
      CLEARTYPE_QUALITY,            // nQuality (Forces ClearType)
      DEFAULT_PITCH | FF_DONTCARE,  // nPitchAndFamily
      kDialogFont                   // lpszFacename
      ));

  // Tell all child controls to use the new font by default.
  SendMessageToDescendants(hwnd(), WM_SETFONT,
                           reinterpret_cast<WPARAM>(default_font_.get()), TRUE);

  const int header_height = ::MulDiv(18, dpi_val, 72);
  header_font_.reset(::CreateFontW(
      -header_height, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, 0, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE, kDialogFont));
  SetItemFont(hwnd(), IDC_INSTALLER_STATE_TEXT, header_font_.get());

  const int body_height = ::MulDiv(16, dpi_val, 72);
  font_.reset(::CreateFontW(-body_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_DONTCARE, kDialogFont));
  SetItemFont(hwnd(), IDC_INFO_TEXT, font_.get());
  SetItemFont(hwnd(), IDC_COMPLETE_TEXT, font_.get());
  SetItemFont(hwnd(), IDC_ERROR_TEXT, font_.get());

  UpdateWindowIcon(current_logo_big_, current_logo_small_, effective_dpi,
                   current_badge_resource_id_);
}

bool OmahaWnd::OnComplete() {
  if (!IsWindow()) {
    RequestExitProcess();
    return false;
  }

  is_complete_ = true;

  EnableClose(true);

  return true;
}

void OmahaWnd::SetControlAttributes(int control_id,
                                    const ControlAttributes& attributes) {
  if (attributes.is_ignore_entry) {
    return;
  }

  HWND ctl = ::GetDlgItem(hwnd(), control_id);
  CHECK(ctl);
  ::ShowWindow(ctl, attributes.is_visible ? SW_SHOW : SW_HIDE);
  ::EnableWindow(ctl, attributes.is_enabled);
  if (attributes.is_button && attributes.is_default) {
    // Ask the dialog manager to give the default push button the focus, so
    // that the <Enter> key works as expected.
    GotoDlgCtrl(hwnd(), ctl);
    LONG style = ::GetWindowLong(ctl, GWL_STYLE);
    if (style) {
      style |= BS_DEFPUSHBUTTON;
      ::SetWindowLong(ctl, GWL_STYLE, style);
    }
  }
}

HRESULT OmahaWnd::EnableClose(bool enable) {
  is_close_enabled_ = enable;
  return EnableSystemCloseButton(is_close_enabled_);
}

HRESULT OmahaWnd::EnableSystemCloseButton(bool enable) {
  HMENU menu = ::GetSystemMenu(hwnd(), FALSE);
  VLOG_IF(2, !menu) << "Failed to find system menu";
  if (!menu) {
    return E_FAIL;
  }
  ::EnableMenuItem(menu, SC_CLOSE,
                   MF_BYCOMMAND | (enable ? MF_ENABLED : MF_GRAYED));
  RecalcLayout(hwnd(), ::GetDlgItem(hwnd(), IDC_TITLE_BAR_SPACER));
  return S_OK;
}

HRESULT InitializeCommonControls(DWORD control_classes) {
  INITCOMMONCONTROLSEX init_ctrls = {sizeof(INITCOMMONCONTROLSEX), 0};
  CHECK_EQ(init_ctrls.dwSize, sizeof(init_ctrls));
  init_ctrls.dwICC = control_classes;
  if (!::InitCommonControlsEx(&init_ctrls)) {
    const DWORD error = ::GetLastError();
    if (error != ERROR_CLASS_ALREADY_EXISTS) {
      return HRESULT_FROM_WIN32(error);
    }
  }
  return S_OK;
}

}  // namespace updater::ui
