// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/owner_draw_controls.h"

#include <windows.h>

#include <commctrl.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/current_module.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "chrome/updater/win/ui/ui_util.h"
#include "ui/gfx/geometry/rect.h"

namespace updater::ui {

namespace {

int Width(const RECT& rect) {
  return rect.right - rect.left;
}

int Height(const RECT& rect) {
  return rect.bottom - rect.top;
}

bool IsRectEmpty(const RECT& rect) {
  return Width(rect) <= 0 || Height(rect) <= 0;
}

void DeflateRect(RECT* rect, int dx, int dy) {
  rect->left += dx;
  rect->top += dy;
  rect->right -= dx;
  rect->bottom -= dy;
}

void FillSolidRect(HDC dc, const RECT& rect, COLORREF color) {
  const COLORREF old_bk = ::SetBkColor(dc, color);
  ::ExtTextOutW(dc, 0, 0, ETO_OPAQUE, &rect, L"", 0, nullptr);
  ::SetBkColor(dc, old_bk);
}

// Draws the parent's background (the gradient) onto a child's DC.
void DrawParentBackground(HWND hwnd, HDC hdc, const RECT& rect) {
  const HWND parent_hwnd = ::GetParent(hwnd);
  if (!parent_hwnd) {
    return;
  }

  POINT pt = {0, 0};
  if (!::MapWindowPoints(hwnd, parent_hwnd, &pt, 1) &&
      ::GetLastError() != ERROR_SUCCESS) {
    return;
  }

  // Offset the DC so the parent draws the gradient at the correct coordinates
  // relative to the child's position.
  if (!::OffsetWindowOrgEx(hdc, pt.x, pt.y, &pt)) {
    return;
  }
  ::SendMessage(parent_hwnd, WM_ERASEBKGND, reinterpret_cast<WPARAM>(hdc), 0);

  // Restore the original origin.
  ::SetWindowOrgEx(hdc, pt.x, pt.y, nullptr);
}

// Returns the system color corresponding to `high_contrast_color_index` if the
// system is in high contrast mode. Otherwise, it returns `normal_color`.
COLORREF GetColor(COLORREF normal_color, int high_contrast_color_index) {
  return IsHighContrastOn() ? ::GetSysColor(high_contrast_color_index)
                            : normal_color;
}

// Returns the system color brush corresponding to `high_contrast_color_index`
// if the system is in high contrast mode. Otherwise, it returns `normal_brush`.
HBRUSH GetColorBrush(HBRUSH normal_brush, int high_contrast_color_index) {
  return IsHighContrastOn() ? ::GetSysColorBrush(high_contrast_color_index)
                            : normal_brush;
}

gfx::Rect RectToGfx(const RECT& rc) {
  return gfx::Rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
}

}  // namespace

CaptionButton::CaptionButton()
    : foreground_brush_(::CreateSolidBrush(kCaptionForegroundColor)) {}

CaptionButton::~CaptionButton() {
  if (tool_tip_window_ && ::IsWindow(tool_tip_window_)) {
    ::DestroyWindow(tool_tip_window_);
    tool_tip_window_ = nullptr;
  }
}

HWND CaptionButton::Create(HWND parent, const RECT& bounds, int control_id) {
  // Use the system `BUTTON` class so the control participates in MSAA/UIA
  // (announced as a push button) and natively handles keyboard activation
  // and `BN_CLICKED` dispatch. The localized tool-tip string also serves as
  // the accessible name (window text). `BS_OWNERDRAW` suppresses the default
  // `BUTTON` paint (including the focus rectangle and any default-button
  // frame); the parent dispatches `WM_DRAWITEM` back to `DrawItem` so the
  // custom icon is rendered.
  HWND control_hwnd = ::CreateWindowExW(
      0, L"BUTTON", tool_tip_text_.c_str(),
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, bounds.left,
      bounds.top, Width(bounds), Height(bounds), parent,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(control_id)),
      CURRENT_MODULE(), nullptr);
  CHECK(control_hwnd && ::IsWindow(control_hwnd));
  CHECK(SubclassWindow(control_hwnd));

  // The `BUTTON`'s `WM_CREATE` has already fired by the time the subclass is
  // installed, so set the tool tip up here rather than from a `WM_CREATE`
  // handler.
  CHECK(!tool_tip_text_.empty());
  tool_tip_window_ = ::CreateWindowExW(
      0, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP, CW_USEDEFAULT,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd(), nullptr,
      CURRENT_MODULE(), nullptr);
  CHECK(tool_tip_window_ && ::IsWindow(tool_tip_window_));

  ::SendMessageW(tool_tip_window_, TTM_SETDELAYTIME, TTDT_AUTOMATIC,
                 MAKELONG(2000, 0));
  ::SendMessageW(tool_tip_window_, TTM_ACTIVATE, TRUE, 0);

  TOOLINFOW ti = {
      .cbSize = sizeof(TOOLINFOW),
      .uFlags = TTF_IDISHWND | TTF_SUBCLASS,
      .hwnd = hwnd(),
      .uId = reinterpret_cast<UINT_PTR>(hwnd()),
      .lpszText = const_cast<LPWSTR>(tool_tip_text_.c_str()),
  };
  ::SendMessageW(tool_tip_window_, TTM_ADDTOOLW, 0,
                 reinterpret_cast<LPARAM>(&ti));

  return hwnd();
}

LRESULT CaptionButton::OnMouseMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
  if (tool_tip_window_ && ::IsWindow(tool_tip_window_)) {
    MSG relay_msg = {hwnd(), msg, wparam, lparam};
    ::SendMessageW(tool_tip_window_, TTM_RELAYEVENT, 0,
                   reinterpret_cast<LPARAM>(&relay_msg));
  }
  // Mouse messages must continue to the default `BUTTON` procedure so that
  // it can track pressed state, capture the mouse, and fire `BN_CLICKED`.
  SetMsgHandled(FALSE);
  return 1;
}

LRESULT CaptionButton::OnMouseMove(UINT, WPARAM, LPARAM) {
  if (!is_tracking_mouse_events_) {
    TRACKMOUSEEVENT tme = {};
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_HOVER | TME_LEAVE;
    tme.hwndTrack = hwnd();
    tme.dwHoverTime = 1;
    is_tracking_mouse_events_ = _TrackMouseEvent(&tme);
  }

  // Let `BUTTON`'s default procedure see the move so it can update pressed
  // state and capture state correctly when the user drags out of the button.
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT CaptionButton::OnMouseHover(UINT, WPARAM, LPARAM) {
  if (!is_mouse_hovering_) {
    is_mouse_hovering_ = true;
    ::InvalidateRect(hwnd(), nullptr, FALSE);
    ::UpdateWindow(hwnd());
  }
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT CaptionButton::OnMouseLeave(UINT, WPARAM, LPARAM) {
  TRACKMOUSEEVENT tme = {};
  tme.cbSize = sizeof(TRACKMOUSEEVENT);
  tme.dwFlags = TME_CANCEL | TME_HOVER | TME_LEAVE;
  tme.hwndTrack = hwnd();
  _TrackMouseEvent(&tme);

  is_tracking_mouse_events_ = false;
  is_mouse_hovering_ = false;

  ::InvalidateRect(hwnd(), nullptr, FALSE);
  ::UpdateWindow(hwnd());

  SetMsgHandled(FALSE);
  return 0;
}

void CaptionButton::DrawItem(LPDRAWITEMSTRUCT draw_item_struct) {
  HDC dc = draw_item_struct->hDC;

  RECT button_rect = {};
  ::GetClientRect(hwnd(), &button_rect);

  if (is_mouse_hovering_) {
    // Keep the hover highlight solid.
    FillSolidRect(dc, button_rect, GetColor(kCaptionBkHover, COLOR_HIGHLIGHT));
  } else {
    // Draw the parent's gradient background.
    DrawParentBackground(hwnd(), dc, button_rect);
  }

  int rgn_width = Width(button_rect) * 12 / 31;
  int rgn_height = Height(button_rect) * 12 / 31;
  base::win::ScopedGDIObject<HRGN> rgn(GetButtonRgn(rgn_width, rgn_height));

  // Center the button in the outer button rect.
  ::OffsetRgn(rgn.get(), (Width(button_rect) - rgn_width) / 2,
              (Height(button_rect) - rgn_height) / 2);

  ::FillRgn(
      dc, rgn.get(),
      GetColorBrush(foreground_brush_.get(),
                    is_mouse_hovering_ ? COLOR_HIGHLIGHTTEXT : COLOR_BTNTEXT));

  const UINT button_state = draw_item_struct->itemState;
  if (!(button_state & ODS_FOCUS) || !(button_state & ODS_SELECTED)) {
    return;
  }

  // Draw a scaled frame for the active/focused state.
  base::win::ScopedGDIObject<HPEN> pen(
      ::CreatePen(PS_INSIDEFRAME,
                  /*thickness=*/
                  std::max(1, ::MulDiv(1, /*dpi=*/::GetDpiForWindow(hwnd()),
                                       USER_DEFAULT_SCREEN_DPI)),
                  GetColor(kCaptionFrameColor, COLOR_WINDOWFRAME)));
  const HPEN old_pen = static_cast<HPEN>(::SelectObject(dc, pen.get()));
  const HBRUSH old_brush =
      static_cast<HBRUSH>(::SelectObject(dc, ::GetStockObject(NULL_BRUSH)));

  ::Rectangle(dc, button_rect.left, button_rect.top, button_rect.right,
              button_rect.bottom);

  ::SelectObject(dc, old_brush);
  ::SelectObject(dc, old_pen);
}

COLORREF CaptionButton::bk_color() const {
  return bk_color_;
}

void CaptionButton::set_bk_color(COLORREF bk_color) {
  bk_color_ = bk_color;
}

const std::wstring& CaptionButton::tool_tip_text() const {
  return tool_tip_text_;
}

void CaptionButton::set_tool_tip_text(const std::wstring& tool_tip_text) {
  tool_tip_text_ = tool_tip_text;
}

CloseButton::CloseButton() {
  set_tool_tip_text(GetLocalizedString(IDS_CLOSE_BUTTON_BASE,
                                       base::UTF8ToWide(GetTagLanguage())));
}

HRGN CloseButton::GetButtonRgn(int rgn_width, int rgn_height) {
  // Ensure we have a valid, drawable area. `std::numeric_limits<short>::max()`
  // is a safe UI upper bound.
  if (rgn_width <= 0 || rgn_height <= 0 ||
      rgn_width > std::numeric_limits<short>::max() ||
      rgn_height > std::numeric_limits<short>::max()) {
    return nullptr;
  }

  // Scale the thickness. For instance, 2px at 100% (96 DPI) becomes 4px at 200%
  // (192 DPI).
  const int thickness = std::max(
      1,
      ::MulDiv(2, /*dpi=*/::GetDpiForWindow(hwnd()), USER_DEFAULT_SCREEN_DPI));
  const int center_size = thickness * 2;

  const int square_side = std::min(rgn_width, rgn_height) / 2 * 2;
  const int center_point = square_side / 2;

  // Create the single rectangular center square using the scaled size.
  RECT center_rect = {0, 0, center_size, center_size};
  ::OffsetRect(&center_rect, center_point - thickness,
               center_point - thickness);
  HRGN rgn = ::CreateRectRgnIndirect(&center_rect);

  // Criss-crossing overlapping rectangles form the close button.
  const int loop_limit = square_side - thickness;
  if (loop_limit < 0) {
    // Button is too small to draw the cross.
    return rgn;
  }

  for (int i = 0; i <= loop_limit; ++i) {
    // Top-left to bottom-right.
    base::win::ScopedGDIObject<HRGN> rgn_nw_to_se(
        ::CreateRectRgn(i, i, i + thickness, i + thickness));
    ::CombineRgn(rgn, rgn, rgn_nw_to_se.get(), RGN_OR);

    // Bottom-left to top-right.
    base::win::ScopedGDIObject<HRGN> rgn_sw_to_ne(::CreateRectRgn(
        i, square_side - i - thickness, i + thickness, square_side - i));
    ::CombineRgn(rgn, rgn, rgn_sw_to_ne.get(), RGN_OR);
  }

  ::OffsetRgn(rgn, (rgn_width - square_side) / 2,
              (rgn_height - square_side) / 2);
  return rgn;
}

MinimizeButton::MinimizeButton() {
  set_tool_tip_text(GetLocalizedString(IDS_MINIMIZE_BUTTON_BASE,
                                       base::UTF8ToWide(GetTagLanguage())));
}

HRGN MinimizeButton::GetButtonRgn(int rgn_width, int rgn_height) {
  if (rgn_width <= 0 || rgn_height <= 0 ||
      rgn_width > std::numeric_limits<short>::max() ||
      rgn_height > std::numeric_limits<short>::max()) {
    return nullptr;
  }

  const int thickness = std::max(
      1,
      ::MulDiv(2, /*dpi=*/::GetDpiForWindow(hwnd()), USER_DEFAULT_SCREEN_DPI));

  // Prevent thickness from exceeding total height.
  const int safe_thickness = std::min(thickness, rgn_height);

  // Calculate the vertical center.
  const int y_offset = (rgn_height - safe_thickness) / 2;

  // The Minimize button is a single rectangle. Center it vertically.
  RECT minimize_button_rect = {0, 0, rgn_width, safe_thickness};
  ::OffsetRect(&minimize_button_rect, 0, y_offset);

  return ::CreateRectRgnIndirect(&minimize_button_rect);
}

MaximizeButton::MaximizeButton() {
  // Maximize button is not used.
  set_tool_tip_text(L"");
}

HRGN MaximizeButton::GetButtonRgn(int rgn_width, int rgn_height) {
  const RECT maximize_button_rects[] = {{0, 0, rgn_width, rgn_height},
                                        {1, 2, rgn_width - 1, rgn_height - 1}};

  HRGN rgn = ::CreateRectRgnIndirect(&maximize_button_rects[0]);
  base::win::ScopedGDIObject<HRGN> rgn_temp(
      ::CreateRectRgnIndirect(&maximize_button_rects[1]));
  ::CombineRgn(rgn, rgn, rgn_temp.get(), RGN_DIFF);
  return rgn;
}

OwnerDrawTitleBarWindow::OwnerDrawTitleBarWindow() {
  set_window_style(WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
  // `WS_EX_CONTROLPARENT` lets the dialog manager descend through this
  // intermediate child window when walking the tab order, so the caption
  // buttons it owns become reachable via the Tab key.
  set_window_ex_style(WS_EX_CONTROLPARENT);
  set_initial_class_style(CS_HREDRAW | CS_VREDRAW);
}

OwnerDrawTitleBarWindow::~OwnerDrawTitleBarWindow() = default;

HWND OwnerDrawTitleBarWindow::Create(HWND parent, const RECT& bounds) {
  Init(parent, RectToGfx(bounds));
  return hwnd();
}

LRESULT OwnerDrawTitleBarWindow::OnCreate(UINT, WPARAM, LPARAM) {
  CreateCaptionButtons();
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnDestroy(UINT, WPARAM, LPARAM) {
  if (close_button_.IsWindow()) {
    ::DestroyWindow(close_button_.hwnd());
  }

  if (minimize_button_.IsWindow()) {
    ::DestroyWindow(minimize_button_.hwnd());
  }

  SetMsgHandled(FALSE);
  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnMouseMove(UINT, WPARAM wparam, LPARAM) {
  SetMsgHandled(FALSE);
  if (current_drag_position_.x == -1 || wparam != MK_LBUTTON) {
    return 0;
  }

  POINT pt = {};
  ::GetCursorPos(&pt);
  int dx = pt.x - current_drag_position_.x;
  int dy = pt.y - current_drag_position_.y;
  current_drag_position_ = pt;

  MoveWindowToDragPosition(::GetParent(hwnd()), dx, dy);

  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnLButtonDown(UINT, WPARAM, LPARAM) {
  ::GetCursorPos(&current_drag_position_);
  ::SetCapture(hwnd());
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnLButtonUp(UINT, WPARAM, LPARAM) {
  current_drag_position_.x = -1;
  current_drag_position_.y = -1;
  ::ReleaseCapture();

  // Reset the parent to be the active window.
  ::SetActiveWindow(::GetParent(hwnd()));
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnEraseBkgnd(UINT, WPARAM wparam, LPARAM) {
  RECT rect = {};
  ::GetClientRect(hwnd(), &rect);

  // Draw the parent's gradient background.
  DrawParentBackground(hwnd(), reinterpret_cast<HDC>(wparam), rect);
  return 1;
}

LRESULT OwnerDrawTitleBarWindow::OnSize(UINT, WPARAM, LPARAM) {
  // Recalculate button positions based on new height/width.
  RecalcLayout();

  // Force a redraw to clear artifacts.
  ::InvalidateRect(hwnd(), nullptr, TRUE);

  SetMsgHandled(FALSE);
  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnDrawItem(UINT, WPARAM, LPARAM lparam) {
  LPDRAWITEMSTRUCT dis = reinterpret_cast<LPDRAWITEMSTRUCT>(lparam);
  if (!dis) {
    SetMsgHandled(FALSE);
    return 0;
  }
  if (dis->hwndItem == close_button_.hwnd() && close_button_.IsWindow()) {
    close_button_.DrawItem(dis);
    return TRUE;
  }
  if (dis->hwndItem == minimize_button_.hwnd() && minimize_button_.IsWindow()) {
    minimize_button_.DrawItem(dis);
    return TRUE;
  }
  SetMsgHandled(FALSE);
  return 0;
}

void OwnerDrawTitleBarWindow::OnClose(UINT, int, HWND) {
  ::PostMessage(::GetParent(hwnd()), WM_SYSCOMMAND, MAKEWPARAM(SC_CLOSE, 0), 0);
}

void OwnerDrawTitleBarWindow::OnMaximize(UINT, int, HWND) {
  ::PostMessage(::GetParent(hwnd()), WM_SYSCOMMAND, MAKEWPARAM(SC_CLOSE, 0), 0);
}

void OwnerDrawTitleBarWindow::OnMinimize(UINT, int, HWND) {
  ::PostMessage(::GetParent(hwnd()), WM_SYSCOMMAND, MAKEWPARAM(SC_MINIMIZE, 0),
                0);
}

void OwnerDrawTitleBarWindow::CreateCaptionButtons() {
  close_button_.set_bk_color(bk_color_);
  minimize_button_.set_bk_color(bk_color_);

  // Get the DPI for this specific window.
  const int dpi = ::GetDpiForWindow(hwnd());

  // Use the DPI-aware version of system metrics.
  RECT button_rect = {0, 0, ::GetSystemMetricsForDpi(SM_CXSIZE, dpi),
                      ::GetSystemMetricsForDpi(SM_CYSIZE, dpi)};

  minimize_button_.Create(hwnd(), button_rect, kButtonMinimize);
  close_button_.Create(hwnd(), button_rect, kButtonClose);
  RecalcLayout();
}

void OwnerDrawTitleBarWindow::UpdateButtonState(HMENU menu,
                                                UINT button_sc_id,
                                                const int button_margin,
                                                CaptionButton* button,
                                                RECT* button_rect) {
  CHECK(button);
  CHECK(button_rect);

  if (!button->IsWindow()) {
    return;
  }

  int state = -1;
  if (menu != nullptr && ::IsMenu(menu)) {
    state = static_cast<int>(::GetMenuState(menu, button_sc_id, MF_BYCOMMAND));
  }

  if (state == -1) {
    ::ShowWindow(button->hwnd(), SW_HIDE);
    return;
  }

  ::EnableWindow(button->hwnd(), !(state & (MF_GRAYED | MF_DISABLED)));
  ::SetWindowPos(button->hwnd(), nullptr, button_rect->left, button_rect->top,
                 Width(*button_rect), Height(*button_rect),
                 SWP_NOZORDER | SWP_SHOWWINDOW);
  ::OffsetRect(button_rect, -Width(*button_rect) - button_margin, 0);
}

void OwnerDrawTitleBarWindow::RecalcLayout() {
  RECT title_bar_rect = {};
  ::GetClientRect(hwnd(), &title_bar_rect);

  const int button_margin = Height(title_bar_rect) / 5;
  DeflateRect(&title_bar_rect, button_margin, button_margin);

  const int button_height = Height(title_bar_rect);
  const int button_width = button_height;

  // Position controls from the Close button to the Minimize button.
  RECT button_rect = {title_bar_rect.right - button_width, title_bar_rect.top,
                      title_bar_rect.right, title_bar_rect.bottom};

  HMENU menu = ::GetSystemMenu(::GetParent(hwnd()), FALSE);
  UpdateButtonState(menu, SC_CLOSE, button_margin, &close_button_,
                    &button_rect);
  UpdateButtonState(menu, SC_MINIMIZE, button_margin, &minimize_button_,
                    &button_rect);
}

void OwnerDrawTitleBarWindow::MoveWindowToDragPosition(HWND hwnd,
                                                       int dx,
                                                       int dy) {
  RECT rect = {};
  ::GetWindowRect(hwnd, &rect);

  ::OffsetRect(&rect, dx, dy);
  ::SetWindowPos(hwnd, nullptr, rect.left, rect.top, 0, 0,
                 SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

COLORREF OwnerDrawTitleBarWindow::bk_color() const {
  return bk_color_;
}

void OwnerDrawTitleBarWindow::set_bk_color(COLORREF bk_color) {
  bk_color_ = bk_color;
}

OwnerDrawTitleBar::OwnerDrawTitleBar() = default;

OwnerDrawTitleBar::~OwnerDrawTitleBar() = default;

void OwnerDrawTitleBar::CreateOwnerDrawTitleBar(HWND parent_hwnd,
                                                HWND title_bar_spacer_hwnd,
                                                COLORREF bk_color) {
  CHECK(parent_hwnd);

  RECT title_bar_client_rect =
      ComputeTitleBarClientRect(parent_hwnd, title_bar_spacer_hwnd);

  // This title bar is a child window and occupies the top portion of the parent
  // dialog box window. DS_MODALFRAME and WS_BORDER are incompatible with this
  // title bar.
  const LONG parent_style = ::GetWindowLong(parent_hwnd, GWL_STYLE);
  CHECK(!(parent_style & DS_MODALFRAME));
  CHECK(!(parent_style & WS_BORDER));

  title_bar_window_.set_bk_color(bk_color);
  title_bar_window_.Create(parent_hwnd, title_bar_client_rect);
}

void OwnerDrawTitleBar::RecalcLayout(HWND parent_hwnd,
                                     HWND title_bar_spacer_hwnd) {
  if (!title_bar_window_.IsWindow()) {
    return;
  }

  // Re-compute where the title bar window should be based on the spacer.
  RECT new_rect = ComputeTitleBarClientRect(parent_hwnd, title_bar_spacer_hwnd);

  // Resize the title bar window itself.
  ::SetWindowPos(title_bar_window_.hwnd(), nullptr, new_rect.left, new_rect.top,
                 new_rect.right - new_rect.left, new_rect.bottom - new_rect.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);

  // Now tell the window to reposition its internal buttons.
  title_bar_window_.RecalcLayout();
}

RECT OwnerDrawTitleBar::ComputeTitleBarClientRect(HWND parent_hwnd,
                                                  HWND title_bar_spacer_hwnd) {
  CHECK(parent_hwnd);

  RECT parent_client_rect = {};
  ::GetClientRect(parent_hwnd, &parent_client_rect);
  RECT title_bar_client_rect = parent_client_rect;

  RECT title_bar_spacer_client_rect = {};
  ::GetClientRect(title_bar_spacer_hwnd, &title_bar_spacer_client_rect);
  const int title_bar_height = Height(title_bar_spacer_client_rect);

  title_bar_client_rect.bottom = title_bar_client_rect.top + title_bar_height;

  return title_bar_client_rect;
}

CustomDlgColors::CustomDlgColors() = default;
CustomDlgColors::~CustomDlgColors() = default;

void CustomDlgColors::SetCustomDlgColors(COLORREF text_color,
                                         COLORREF bk_color) {
  text_color_ = text_color;
  bk_color_ = bk_color;
  bk_brush_.reset(::CreateSolidBrush(bk_color_));
}

BOOL CustomDlgColors::ProcessWindowMessage(HWND,
                                           UINT msg,
                                           WPARAM wparam,
                                           LPARAM,
                                           LRESULT& result,
                                           DWORD) {
  if (msg != WM_CTLCOLORDLG && msg != WM_CTLCOLORSTATIC &&
      msg != WM_CTLCOLORBTN) {
    return FALSE;
  }
  HDC dc = reinterpret_cast<HDC>(wparam);

  if (IsHighContrastOn()) {
    int text_color_idx =
        (msg == WM_CTLCOLORBTN) ? COLOR_BTNTEXT : COLOR_WINDOWTEXT;
    int bk_color_idx = (msg == WM_CTLCOLORBTN) ? COLOR_BTNFACE : COLOR_WINDOW;
    ::SetTextColor(dc, ::GetSysColor(text_color_idx));
    ::SetBkColor(dc, ::GetSysColor(bk_color_idx));
    result = reinterpret_cast<LRESULT>(::GetSysColorBrush(bk_color_idx));
    return TRUE;
  }

  if (IsDarkModeOn()) {
    COLORREF text_color =
        (msg == WM_CTLCOLORBTN) ? RGB(0xA8, 0xC7, 0xFA) : RGB(0xFF, 0xFF, 0xFF);
    ::SetTextColor(dc, text_color);
    ::SetBkColor(dc, RGB(0x20, 0x20, 0x20));
    if (!dark_bk_brush_.is_valid()) {
      dark_bk_brush_.reset(::CreateSolidBrush(RGB(0x20, 0x20, 0x20)));
    }
    result = reinterpret_cast<LRESULT>(dark_bk_brush_.get());
    return TRUE;
  }

  // Light Mode
  if (msg == WM_CTLCOLORBTN) {
    ::SetTextColor(dc, GetColor(text_color_, COLOR_BTNTEXT));
    ::SetBkColor(dc, GetColor(bk_color_, COLOR_BTNFACE));
    result = reinterpret_cast<LRESULT>(::GetStockObject(NULL_BRUSH));
    return TRUE;
  }

  ::SetBkColor(dc, GetColor(bk_color_, COLOR_WINDOW));
  ::SetTextColor(dc, GetColor(text_color_, COLOR_WINDOWTEXT));
  result =
      reinterpret_cast<LRESULT>(GetColorBrush(bk_brush_.get(), COLOR_WINDOW));
  return TRUE;
}

CustomProgressBarCtrl::CustomProgressBarCtrl() = default;

CustomProgressBarCtrl::~CustomProgressBarCtrl() = default;

LRESULT CustomProgressBarCtrl::OnEraseBkgnd(UINT, WPARAM, LPARAM) {
  // The background and foreground are both rendered in OnPaint().
  return 1;
}

void CustomProgressBarCtrl::GradientFill(HDC dc,
                                         const RECT& rect,
                                         COLORREF top_color,
                                         COLORREF bottom_color) {
  TRIVERTEX tri_vertex[] = {
      {rect.left, rect.top, static_cast<COLOR16>(GetRValue(top_color) << 8),
       static_cast<COLOR16>(GetGValue(top_color) << 8),
       static_cast<COLOR16>(GetBValue(top_color) << 8), 0},
      {rect.right, rect.bottom,
       static_cast<COLOR16>(GetRValue(bottom_color) << 8),
       static_cast<COLOR16>(GetGValue(bottom_color) << 8),
       static_cast<COLOR16>(GetBValue(bottom_color) << 8), 0},
  };

  GRADIENT_RECT gradient_rect = {0, 1};

  ::GradientFill(dc, tri_vertex, 2, &gradient_rect, 1, GRADIENT_FILL_RECT_V);
}

LRESULT CustomProgressBarCtrl::OnPaint(UINT, WPARAM, LPARAM) {
  PAINTSTRUCT ps = {};
  HDC dc_paint = ::BeginPaint(hwnd(), &ps);
  RECT window_rect = {};
  ::GetClientRect(hwnd(), &window_rect);

  // Calculate a half-width rectangle.
  RECT client_rect = window_rect;
  const int original_height = Height(window_rect);
  const int slim_height = original_height / 2;
  const int vertical_padding = (original_height - slim_height) / 2;

  // Shrink the top and bottom to center the bar.
  client_rect.top += vertical_padding;
  client_rect.bottom -= vertical_padding;

  // Using an offscreen memory DC eliminates flicker.
  HDC dc_mem = ::CreateCompatibleDC(dc_paint);
  base::win::ScopedGDIObject<HBITMAP> bmp_mem(::CreateCompatibleBitmap(
      dc_paint, Width(window_rect), Height(window_rect)));
  HGDIOBJ old_mem_bmp = ::SelectObject(dc_mem, bmp_mem.get());

  // Draw the parent's gradient background into the memory DC first.
  DrawParentBackground(hwnd(), dc_mem, window_rect);

  // Draw at 4x scale to get smooth edges when scaling back down.
  constexpr int kScale = 4;
  RECT high_res_rect = {0, 0, Width(client_rect) * kScale,
                        Height(client_rect) * kScale};

  HDC dc_hi_res = ::CreateCompatibleDC(dc_mem);

  base::win::ScopedGDIObject<HBITMAP> bmp_hi_res(::CreateCompatibleBitmap(
      dc_mem, Width(high_res_rect), Height(high_res_rect)));
  HGDIOBJ old_bmp = ::SelectObject(dc_hi_res, bmp_hi_res.get());

  // The track is the rounded "pill" that holds the progress fill. In light
  // mode it is `empty_fill_color_` (defaulting to `kProgressEmptyFillColor`,
  // i.e. white) so the pill stands out as a slightly brighter shape over the
  // off-white frame drawn at the corners. In high-contrast mode it tracks
  // `COLOR_WINDOW`. In dark mode it is a darker gray that matches the intent
  // of the dark-mode commit `514423a7eb35a` while remaining distinct from the
  // even darker dialog background. (The original dark-mode override never
  // took visual effect in the legacy WTL build because the brush was
  // selected into the wrong DC; now that the selection is correct, the
  // intended dark track color renders.)
  COLORREF track_color = GetColor(empty_fill_color_, COLOR_WINDOW);
  if (!IsHighContrastOn() && IsDarkModeOn()) {
    track_color = RGB(0x44, 0x47, 0x46);
  }

  // `outside_pill_color` fills the area of the high-res bitmap that falls
  // outside the rounded pill. Because the pill is rounded, only the four
  // corners expose this color, and it becomes the pill's visible frame
  // against the dialog background.
  COLORREF outside_pill_color = kProgressEmptyFrameColor;
  if (IsHighContrastOn()) {
    outside_pill_color = ::GetSysColor(COLOR_WINDOW);
  } else if (IsDarkModeOn()) {
    outside_pill_color = RGB(0x20, 0x20, 0x20);
  }

  FillSolidRect(dc_hi_res, high_res_rect, outside_pill_color);

  // Setup GDI objects for rounded drawing. In dark/light mode, suppress the pen
  // entirely (`NULL_PEN`) so the pill is purely a colored fill without borders.
  // In high-contrast mode, stroke the perimeter with `COLOR_WINDOWTEXT` so it's
  // visible.
  base::win::ScopedGDIObject<HPEN> hc_pen;
  HGDIOBJ old_pen = nullptr;
  if (IsHighContrastOn()) {
    hc_pen.reset(
        ::CreatePen(PS_SOLID, kScale, ::GetSysColor(COLOR_WINDOWTEXT)));
    old_pen = ::SelectObject(dc_hi_res, hc_pen.get());
  } else {
    old_pen = ::SelectObject(dc_hi_res, ::GetStockObject(NULL_PEN));
  }

  const int corner_size = std::min(
      Height(high_res_rect), ::MulDiv(16 * kScale, ::GetDpiForWindow(hwnd()),
                                      USER_DEFAULT_SCREEN_DPI));

  // Draw the Background Track.
  base::win::ScopedGDIObject<HBRUSH> bg_brush(::CreateSolidBrush(track_color));
  HGDIOBJ old_brush = ::SelectObject(dc_hi_res, bg_brush.get());
  ::RoundRect(dc_hi_res, high_res_rect.left, high_res_rect.top,
              high_res_rect.right, high_res_rect.bottom, corner_size,
              corner_size);

  // Calculate Progress Width.
  const int kBarWidth = kMaxPosition - kMinPosition;
  if (kBarWidth > 0) {
    const LONG bar_rect_right =
        high_res_rect.left +
        Width(high_res_rect) * (current_position_ - kMinPosition) / kBarWidth;

    RECT progress_rect = high_res_rect;
    progress_rect.right = std::min(bar_rect_right, high_res_rect.right);

    // Handle Marquee Style animation.
    if (::GetWindowLong(hwnd(), GWL_STYLE) & PBS_MARQUEE) {
      const LONG bar_rect_left =
          bar_rect_right - (Width(high_res_rect) * kMarqueeWidth / kBarWidth);
      progress_rect.left = std::max(bar_rect_left, high_res_rect.left);
    }

    // Draw the fill.
    if (!IsRectEmpty(progress_rect) &&
        Width(progress_rect) > (corner_size / 2)) {
      base::win::ScopedGDIObject<HBRUSH> fill_brush(
          ::CreateSolidBrush((!IsHighContrastOn() && IsDarkModeOn())
                                 ? RGB(0xA8, 0xC7, 0xFA)
                                 : GetColor(bar_color_, COLOR_HIGHLIGHT)));
      ::SelectObject(dc_hi_res, fill_brush.get());
      ::RoundRect(dc_hi_res, progress_rect.left, progress_rect.top,
                  progress_rect.right, progress_rect.bottom, corner_size,
                  corner_size);
      ::SelectObject(dc_hi_res, bg_brush.get());
    }
  }

  // `HALFTONE` creates a smooth anti-aliased look.
  ::SetStretchBltMode(dc_mem, HALFTONE);

  // Required for HALFTONE to work correctly.
  ::SetBrushOrgEx(dc_mem, 0, 0, nullptr);

  // Scale the high-res bar back down to the screen.
  ::StretchBlt(dc_mem, client_rect.left, client_rect.top, Width(client_rect),
               Height(client_rect), dc_hi_res, 0, 0, Width(high_res_rect),
               Height(high_res_rect), SRCCOPY);

  // Blit the offscreen DC to the paint DC.
  ::BitBlt(dc_paint, window_rect.left, window_rect.top, Width(window_rect),
           Height(window_rect), dc_mem, 0, 0, SRCCOPY);

  // Cleanup.
  ::SelectObject(dc_hi_res, old_brush);
  if (old_pen) {
    ::SelectObject(dc_hi_res, old_pen);
  }
  ::SelectObject(dc_hi_res, old_bmp);
  ::DeleteDC(dc_hi_res);

  ::SelectObject(dc_mem, old_mem_bmp);
  ::DeleteDC(dc_mem);

  ::EndPaint(hwnd(), &ps);
  return 0;
}

LRESULT CustomProgressBarCtrl::OnTimer(UINT, WPARAM event_id, LPARAM) {
  if (event_id != kMarqueeTimerId) {
    SetMsgHandled(FALSE);
    return 0;
  }
  ::SendMessage(hwnd(), PBM_SETPOS, 0, 0L);
  return 0;
}

LRESULT CustomProgressBarCtrl::OnSysColorChange(UINT, WPARAM, LPARAM) {
  ::RedrawWindow(hwnd(), nullptr, nullptr,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT CustomProgressBarCtrl::OnSetPos(UINT, WPARAM new_position, LPARAM) {
  // To allow accessibility to show the correct progress values, pass
  // `PBM_SETPOS` through to the underlying Win32 progress bar.
  SetMsgHandled(FALSE);

  int old_position = current_position_;

  if (::GetWindowLong(hwnd(), GWL_STYLE) & PBS_MARQUEE) {
    current_position_++;
    if (current_position_ >= (kMaxPosition + kMarqueeWidth)) {
      current_position_ = kMinPosition;
    }
  } else {
    current_position_ = std::min(static_cast<int>(new_position), kMaxPosition);
  }

  if (current_position_ < kMinPosition) {
    current_position_ = kMinPosition;
  }

  ::RedrawWindow(hwnd(), nullptr, nullptr,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);

  return old_position;
}

LRESULT CustomProgressBarCtrl::OnSetMarquee(UINT,
                                            WPARAM is_set_marquee,
                                            LPARAM update_msec) {
  // To allow accessibility to show the correct progress values, pass
  // `PBM_SETMARQUEE` through to the underlying Win32 progress bar.
  SetMsgHandled(FALSE);

  if (is_set_marquee && !is_marquee_mode_) {
    current_position_ = kMinPosition;
    ::SetTimer(hwnd(), kMarqueeTimerId, static_cast<UINT>(update_msec),
               nullptr);
    is_marquee_mode_ = true;
  } else if (!is_set_marquee && is_marquee_mode_) {
    ::KillTimer(hwnd(), kMarqueeTimerId);
    is_marquee_mode_ = false;
  }

  ::RedrawWindow(hwnd(), nullptr, nullptr,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);

  return is_set_marquee;
}

// Calling `PBM_SETBARCOLOR` converts the progress bar into a solid colored bar
// (no gradient).
LRESULT CustomProgressBarCtrl::OnSetBarColor(UINT, WPARAM, LPARAM bar_color) {
  COLORREF old_bar_color = bar_color_;
  bar_color_ = static_cast<COLORREF>(bar_color);

  ::RedrawWindow(hwnd(), nullptr, nullptr,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);

  return old_bar_color;
}

LRESULT CustomProgressBarCtrl::OnSetBkColor(UINT,
                                            WPARAM,
                                            LPARAM empty_fill_color) {
  COLORREF old_empty_fill_color = empty_fill_color_;
  empty_fill_color_ = static_cast<COLORREF>(empty_fill_color);

  ::RedrawWindow(hwnd(), nullptr, nullptr,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);

  return old_empty_fill_color;
}

FlatButton::FlatButton() = default;
FlatButton::~FlatButton() = default;

void FlatButton::SetIsPrimary(bool is_primary) {
  is_primary_ = is_primary;
  if (IsWindow()) {
    ::InvalidateRect(hwnd(), nullptr, FALSE);
  }
}

LRESULT FlatButton::OnMouseMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
  SetMsgHandled(FALSE);
  return 1;
}

LRESULT FlatButton::OnMouseMove(UINT, WPARAM, LPARAM) {
  if (!is_tracking_mouse_events_) {
    TRACKMOUSEEVENT tme = {};
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_HOVER | TME_LEAVE;
    tme.hwndTrack = hwnd();
    tme.dwHoverTime = 1;
    is_tracking_mouse_events_ = _TrackMouseEvent(&tme);
  }
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT FlatButton::OnMouseHover(UINT, WPARAM, LPARAM) {
  if (!is_mouse_hovering_) {
    is_mouse_hovering_ = true;
    ::InvalidateRect(hwnd(), nullptr, FALSE);
    ::UpdateWindow(hwnd());
  }
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT FlatButton::OnMouseLeave(UINT, WPARAM, LPARAM) {
  TRACKMOUSEEVENT tme = {};
  tme.cbSize = sizeof(TRACKMOUSEEVENT);
  tme.dwFlags = TME_CANCEL | TME_HOVER | TME_LEAVE;
  tme.hwndTrack = hwnd();
  _TrackMouseEvent(&tme);

  is_tracking_mouse_events_ = false;
  is_mouse_hovering_ = false;

  ::InvalidateRect(hwnd(), nullptr, FALSE);
  ::UpdateWindow(hwnd());

  SetMsgHandled(FALSE);
  return 0;
}

LRESULT FlatButton::OnEraseBkgnd(UINT, WPARAM, LPARAM) {
  return 1;
}

LRESULT FlatButton::OnPaint(UINT, WPARAM, LPARAM) {
  PAINTSTRUCT ps = {};
  HDC dc_paint = ::BeginPaint(hwnd(), &ps);
  RECT rect = {};
  ::GetClientRect(hwnd(), &rect);

  if (IsRectEmpty(rect)) {
    ::EndPaint(hwnd(), &ps);
    return 0;
  }

  HDC dc = ::CreateCompatibleDC(dc_paint);
  base::win::ScopedGDIObject<HBITMAP> bmp(
      ::CreateCompatibleBitmap(dc_paint, Width(rect), Height(rect)));
  if (!dc || !bmp.is_valid()) {
    if (dc) {
      ::DeleteDC(dc);
    }
    ::EndPaint(hwnd(), &ps);
    return 0;
  }
  HGDIOBJ old_bmp = ::SelectObject(dc, bmp.get());

  DrawParentBackground(hwnd(), dc, rect);

  const bool is_disabled = !::IsWindowEnabled(hwnd());
  const bool is_default =
      (::GetWindowLong(hwnd(), GWL_STYLE) & BS_DEFPUSHBUTTON) != 0;
  const bool is_pressed =
      ((::SendMessageW(hwnd(), BM_GETSTATE, 0, 0) & BST_PUSHED) != 0);

  COLORREF bg = CLR_INVALID;
  COLORREF text = CLR_INVALID;
  COLORREF border = CLR_INVALID;

  if (IsHighContrastOn()) {
    bg = (is_pressed || is_default) ? ::GetSysColor(COLOR_HIGHLIGHT)
                                    : ::GetSysColor(COLOR_BTNFACE);
    text = (is_pressed || is_default) ? ::GetSysColor(COLOR_HIGHLIGHTTEXT)
                                      : ::GetSysColor(COLOR_BTNTEXT);
    border = ::GetSysColor(COLOR_WINDOWFRAME);
  } else if (IsDarkModeOn()) {
    if (is_disabled) {
      bg = kButtonBgDisabledDark;
      text = kButtonFgDisabledDark;
      border = bg;
    } else if (is_primary_) {
      bg = is_pressed ? kPrimaryButtonBgDarkPressed
                      : (is_mouse_hovering_ ? kPrimaryButtonBgDarkHover
                                            : kPrimaryButtonBgDark);
      text = kPrimaryButtonFgDark;
      border = bg;
    } else {
      bg = is_pressed ? kSecondaryButtonBgDarkPressed
                      : (is_mouse_hovering_ ? kSecondaryButtonBgDarkHover
                                            : kSecondaryButtonBgDark);
      text = kSecondaryButtonFgDark;
      border = kSecondaryButtonBorderDark;
    }
  } else {
    if (is_disabled) {
      bg = kButtonBgDisabled;
      text = kButtonFgDisabled;
      border = bg;
    } else if (is_primary_) {
      bg = is_pressed ? kPrimaryButtonBgPressed
                      : (is_mouse_hovering_ ? kPrimaryButtonBgHover
                                            : kPrimaryButtonBg);
      text = kPrimaryButtonFg;
      border = bg;
    } else {
      bg = is_pressed ? kSecondaryButtonBgPressed
                      : (is_mouse_hovering_ ? kSecondaryButtonBgHover
                                            : kSecondaryButtonBg);
      text = kSecondaryButtonFg;
      border = is_mouse_hovering_ ? kSecondaryButtonFg : kSecondaryButtonBorder;
    }
  }

  const int dpi = ::GetDpiForWindow(hwnd());
  const int radius_x = ::MulDiv(4, dpi, USER_DEFAULT_SCREEN_DPI);
  const int radius_y = ::MulDiv(4, dpi, USER_DEFAULT_SCREEN_DPI);

  base::win::ScopedGDIObject<HBRUSH> bg_brush(::CreateSolidBrush(bg));
  HGDIOBJ old_brush = ::SelectObject(dc, bg_brush.get());

  HGDIOBJ old_pen = nullptr;
  base::win::ScopedGDIObject<HPEN> border_pen;
  if (border != CLR_INVALID) {
    const int thickness =
        std::max(1, ::MulDiv(1, dpi, USER_DEFAULT_SCREEN_DPI));
    border_pen.reset(::CreatePen(PS_SOLID, thickness, border));
    old_pen = ::SelectObject(dc, border_pen.get());
  } else {
    old_pen = ::SelectObject(dc, ::GetStockObject(NULL_PEN));
  }

  ::RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, 2 * radius_x,
              2 * radius_y);

  if (::GetFocus() == hwnd() && !is_disabled) {
    RECT focus_rect = rect;
    ::InflateRect(&focus_rect, -2, -2);
    ::DrawFocusRect(dc, &focus_rect);
  }

  wchar_t button_text[256] = {};
  ::GetWindowTextW(hwnd(), button_text, std::size(button_text));

  if (wcslen(button_text) > 0) {
    const COLORREF old_text_color = ::SetTextColor(dc, text);
    const int old_bk_mode = ::SetBkMode(dc, TRANSPARENT);

    HFONT font =
        reinterpret_cast<HFONT>(::SendMessageW(hwnd(), WM_GETFONT, 0, 0));
    HFONT old_font = nullptr;
    if (font) {
      old_font = static_cast<HFONT>(::SelectObject(dc, font));
    }

    ::DrawTextW(dc, button_text, -1, &rect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (old_font) {
      ::SelectObject(dc, old_font);
    }
    ::SetBkMode(dc, old_bk_mode);
    ::SetTextColor(dc, old_text_color);
  }

  ::BitBlt(dc_paint, rect.left, rect.top, Width(rect), Height(rect), dc, 0, 0,
           SRCCOPY);

  ::SelectObject(dc, old_brush);
  if (old_pen) {
    ::SelectObject(dc, old_pen);
  }
  ::SelectObject(dc, old_bmp);
  ::DeleteDC(dc);

  ::EndPaint(hwnd(), &ps);
  return 0;
}

}  // namespace updater::ui
