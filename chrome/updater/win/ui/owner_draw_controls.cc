// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/owner_draw_controls.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "chrome/updater/win/ui/ui_util.h"

namespace updater::ui {

namespace {

// Draws the parent's background (the gradient) onto a child's DC.
void DrawParentBackground(HWND hwnd, HDC hdc, const RECT& rect) {
  const HWND parent_hwnd = ::GetParent(hwnd);
  if (!parent_hwnd) {
    return;
  }

  CPoint pt(0, 0);
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
HBRUSH GetColorBrush(const WTL::CBrush& normal_brush,
                     int high_contrast_color_index) {
  return IsHighContrastOn() ? ::GetSysColorBrush(high_contrast_color_index)
                            : HBRUSH{normal_brush};
}

}  // namespace

CaptionButton::CaptionButton() = default;
CaptionButton::~CaptionButton() = default;

LRESULT CaptionButton::OnCreate(UINT, WPARAM, LPARAM, BOOL& handled) {
  handled = false;

  tool_tip_window_.Create(m_hWnd);
  CHECK(tool_tip_window_.IsWindow());
  CHECK(!tool_tip_text_.IsEmpty());

  tool_tip_window_.SetDelayTime(TTDT_AUTOMATIC, 2000);
  tool_tip_window_.Activate(TRUE);
  tool_tip_window_.AddTool(m_hWnd, tool_tip_text_.GetString());

  return 0;
}

LRESULT CaptionButton::OnMouseMessage(UINT msg,
                                      WPARAM wparam,
                                      LPARAM lparam,
                                      BOOL& handled) {
  handled = false;

  if (tool_tip_window_.IsWindow()) {
    MSG relay_msg = {m_hWnd, msg, wparam, lparam};
    tool_tip_window_.RelayEvent(&relay_msg);
  }

  return 1;
}

LRESULT CaptionButton::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL& handled) {
  // The background and foreground are both rendered in DrawItem().
  handled = true;
  return 1;
}

LRESULT CaptionButton::OnMouseMove(UINT, WPARAM, LPARAM, BOOL& handled) {
  if (!is_tracking_mouse_events_) {
    TRACKMOUSEEVENT tme = {};
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_HOVER | TME_LEAVE;
    tme.hwndTrack = m_hWnd;
    tme.dwHoverTime = 1;
    is_tracking_mouse_events_ = _TrackMouseEvent(&tme);
  }

  return 0;
}

LRESULT CaptionButton::OnMouseHover(UINT, WPARAM, LPARAM, BOOL& handled) {
  handled = false;

  if (!is_mouse_hovering_) {
    is_mouse_hovering_ = true;
    Invalidate(false);
    UpdateWindow();
  }

  return 0;
}

LRESULT CaptionButton::OnMouseLeave(UINT, WPARAM, LPARAM, BOOL& handled) {
  handled = false;

  TRACKMOUSEEVENT tme = {};
  tme.cbSize = sizeof(TRACKMOUSEEVENT);
  tme.dwFlags = TME_CANCEL | TME_HOVER | TME_LEAVE;
  tme.hwndTrack = m_hWnd;

  _TrackMouseEvent(&tme);

  is_tracking_mouse_events_ = false;
  is_mouse_hovering_ = false;

  Invalidate(false);
  UpdateWindow();

  return 0;
}

void CaptionButton::DrawItem(LPDRAWITEMSTRUCT draw_item_struct) {
  WTL::CDCHandle dc(draw_item_struct->hDC);

  CRect button_rect;
  GetClientRect(&button_rect);

  if (is_mouse_hovering_) {
    // Keep the hover highlight solid.
    dc.FillSolidRect(&button_rect, GetColor(kCaptionBkHover, COLOR_HIGHLIGHT));
  } else {
    // Draw the parent's gradient background.
    DrawParentBackground(m_hWnd, dc, button_rect);
  }

  int rgn_width = button_rect.Width() * 12 / 31;
  int rgn_height = button_rect.Height() * 12 / 31;
  WTL::CRgn rgn(GetButtonRgn(rgn_width, rgn_height));

  // Center the button in the outer button rect.
  rgn.OffsetRgn((button_rect.Width() - rgn_width) / 2,
                (button_rect.Height() - rgn_height) / 2);

  dc.FillRgn(rgn, GetColorBrush(foreground_brush_, is_mouse_hovering_
                                                       ? COLOR_HIGHLIGHTTEXT
                                                       : COLOR_BTNTEXT));

  const UINT button_state = draw_item_struct->itemState;
  if (!(button_state & ODS_FOCUS) || !(button_state & ODS_SELECTED)) {
    return;
  }

  // Draw a scaled frame for the active/focused state.
  WTL::CPen pen;
  pen.CreatePen(PS_INSIDEFRAME, /*thickness=*/
                std::max(1, ::MulDiv(1, /*dpi=*/::GetDpiForWindow(m_hWnd),
                                     USER_DEFAULT_SCREEN_DPI)),
                GetColor(kCaptionFrameColor, COLOR_WINDOWFRAME));
  const HPEN old_pen = dc.SelectPen(pen);
  const HBRUSH old_brush = dc.SelectBrush((HBRUSH)GetStockObject(NULL_BRUSH));

  dc.Rectangle(&button_rect);

  dc.SelectBrush(old_brush);
  dc.SelectPen(old_pen);
}

COLORREF CaptionButton::bk_color() const {
  return bk_color_;
}

void CaptionButton::set_bk_color(COLORREF bk_color) {
  bk_color_ = bk_color;
}

CString CaptionButton::tool_tip_text() const {
  return tool_tip_text_;
}

void CaptionButton::set_tool_tip_text(const CString& tool_tip_text) {
  tool_tip_text_ = tool_tip_text;
}

CloseButton::CloseButton() {
  set_tool_tip_text(GetLocalizedString(IDS_CLOSE_BUTTON_BASE,
                                       base::UTF8ToWide(GetTagLanguage()))
                        .c_str());
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
      ::MulDiv(2, /*dpi=*/::GetDpiForWindow(m_hWnd), USER_DEFAULT_SCREEN_DPI));
  const int center_size = thickness * 2;

  const int square_side = std::min(rgn_width, rgn_height) / 2 * 2;
  const int center_point = square_side / 2;

  // Create the single rectangular center square using the scaled size.
  CRect center_rect(0, 0, center_size, center_size);
  center_rect.OffsetRect(center_point - thickness, center_point - thickness);
  WTL::CRgnHandle rgn(::CreateRectRgnIndirect(&center_rect));

  // Criss-crossing overlapping rectangles form the close button.
  const int loop_limit = square_side - thickness;
  if (loop_limit < 0) {
    // Button is too small to draw the cross.
    return rgn;
  }

  for (int i = 0; i <= loop_limit; ++i) {
    // Top-left to bottom-right.
    WTL::CRgn rgn_nw_to_se(::CreateRectRgn(i, i, i + thickness, i + thickness));
    rgn.CombineRgn(rgn_nw_to_se, RGN_OR);

    // Bottom-left to top-right.
    WTL::CRgn rgn_sw_to_ne(::CreateRectRgn(i, square_side - i - thickness,
                                           i + thickness, square_side - i));
    rgn.CombineRgn(rgn_sw_to_ne, RGN_OR);
  }

  rgn.OffsetRgn((rgn_width - square_side) / 2, (rgn_height - square_side) / 2);
  return rgn;
}

MinimizeButton::MinimizeButton() {
  set_tool_tip_text(GetLocalizedString(IDS_MINIMIZE_BUTTON_BASE,
                                       base::UTF8ToWide(GetTagLanguage()))
                        .c_str());
}

HRGN MinimizeButton::GetButtonRgn(int rgn_width, int rgn_height) {
  // Ensure we have a valid, drawable area. `std::numeric_limits<short>::max()`
  // is a safe UI upper bound.
  if (rgn_width <= 0 || rgn_height <= 0 ||
      rgn_width > std::numeric_limits<short>::max() ||
      rgn_height > std::numeric_limits<short>::max()) {
    return nullptr;
  }

  // Scale the bar thickness. 2px at 100% becomes 4px at 200%.
  const int thickness = std::max(
      1,
      ::MulDiv(2, /*dpi=*/::GetDpiForWindow(m_hWnd), USER_DEFAULT_SCREEN_DPI));

  // Prevent thickness from exceeding total height. If the bar is thicker than
  // the area, we cap it to the area height.
  const int safe_thickness = std::min(thickness, rgn_height);

  // Calculate the vertical center.
  const int y_offset = (rgn_height - safe_thickness) / 2;

  // The Minimize button is a single rectangle. Center it vertically.
  CRect minimize_button_rect(0, 0, rgn_width, safe_thickness);
  minimize_button_rect.OffsetRect(0, y_offset);

  return ::CreateRectRgnIndirect(&minimize_button_rect);
}

MaximizeButton::MaximizeButton() {
  // Maximize button is not used.
  set_tool_tip_text(L"");
}

HRGN MaximizeButton::GetButtonRgn(int rgn_width, int rgn_height) {
  // Overlapping outer and inner rectangles form the maximize button.
  const RECT maximize_button_rects[] = {{0, 0, rgn_width, rgn_height},
                                        {1, 2, rgn_width - 1, rgn_height - 1}};

  WTL::CRgnHandle rgn(::CreateRectRgnIndirect(&maximize_button_rects[0]));
  WTL::CRgn rgn_temp(::CreateRectRgnIndirect(&maximize_button_rects[1]));
  rgn.CombineRgn(rgn_temp, RGN_DIFF);
  return rgn;
}

OwnerDrawTitleBarWindow::OwnerDrawTitleBarWindow() = default;
OwnerDrawTitleBarWindow::~OwnerDrawTitleBarWindow() = default;

LRESULT OwnerDrawTitleBarWindow::OnCreate(UINT, WPARAM, LPARAM, BOOL& handled) {
  handled = false;

  CreateCaptionButtons();
  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnDestroy(UINT,
                                           WPARAM,
                                           LPARAM,
                                           BOOL& handled) {
  handled = false;

  if (close_button_.IsWindow()) {
    close_button_.DestroyWindow();
  }

  if (minimize_button_.IsWindow()) {
    minimize_button_.DestroyWindow();
  }

  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnMouseMove(UINT,
                                             WPARAM wparam,
                                             LPARAM,
                                             BOOL& handled) {
  handled = false;
  if (current_drag_position_.x == -1 || wparam != MK_LBUTTON) {
    return 0;
  }

  CPoint pt;
  ::GetCursorPos(&pt);
  int dx = pt.x - current_drag_position_.x;
  int dy = pt.y - current_drag_position_.y;
  current_drag_position_ = pt;

  MoveWindowToDragPosition(GetParent(), dx, dy);

  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnLButtonDown(UINT,
                                               WPARAM,
                                               LPARAM,
                                               BOOL& handled) {
  handled = false;
  ::GetCursorPos(&current_drag_position_);
  SetCapture();
  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnLButtonUp(UINT,
                                             WPARAM,
                                             LPARAM,
                                             BOOL& handled) {
  handled = false;

  current_drag_position_.x = -1;
  current_drag_position_.y = -1;
  ReleaseCapture();

  // Reset the parent to be the active window.
  ::SetActiveWindow(GetParent());
  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnEraseBkgnd(UINT,
                                              WPARAM wparam,
                                              LPARAM,
                                              BOOL& handled) {
  handled = true;

  CRect rect;
  GetClientRect(&rect);

  // Draw the parent's gradient background.
  DrawParentBackground(m_hWnd, reinterpret_cast<HDC>(wparam), rect);
  return 1;
}

LRESULT OwnerDrawTitleBarWindow::OnSize(UINT, WPARAM, LPARAM, BOOL& handled) {
  // Recalculate button positions based on new height/width.
  RecalcLayout();

  // Force a redraw to clear artifacts.
  Invalidate();

  handled = false;
  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnClose(WORD, WORD, HWND, BOOL& handled) {
  handled = false;

  ::PostMessage(GetParent(), WM_SYSCOMMAND, MAKEWPARAM(SC_CLOSE, 0), 0);
  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnMaximize(WORD, WORD, HWND, BOOL& handled) {
  handled = false;

  ::PostMessage(GetParent(), WM_SYSCOMMAND, MAKEWPARAM(SC_CLOSE, 0), 0);
  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnMinimize(WORD, WORD, HWND, BOOL& handled) {
  handled = false;

  ::PostMessage(GetParent(), WM_SYSCOMMAND, MAKEWPARAM(SC_MINIMIZE, 0), 0);
  return 0;
}

void OwnerDrawTitleBarWindow::CreateCaptionButtons() {
  close_button_.set_bk_color(bk_color_);
  minimize_button_.set_bk_color(bk_color_);

  // Get the DPI for this specific window
  const int dpi = ::GetDpiForWindow(m_hWnd);

  // Use the DPI-aware version of system metrics.
  CRect button_rect(0, 0, ::GetSystemMetricsForDpi(SM_CXSIZE, dpi),
                    ::GetSystemMetricsForDpi(SM_CYSIZE, dpi));

  minimize_button_.Create(m_hWnd, button_rect, nullptr,
                          WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0,
                          kButtonMinimize);

  close_button_.Create(m_hWnd, button_rect, nullptr,
                       WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, kButtonClose);
  RecalcLayout();
}

// This function handles four button states:
// - Button window does not exist. Nothing to do in this case.
// - Corresponding menu item)does not exist. Hide button.
// - Corresponding menu item disabled. Disable and Show button.
// - Corresponding menu item enabled. Enable and Show button.
void OwnerDrawTitleBarWindow::UpdateButtonState(const WTL::CMenuHandle& menu,
                                                UINT button_sc_id,
                                                const int button_margin,
                                                CaptionButton* button,
                                                CRect* button_rect) {
  CHECK(button);
  CHECK(button_rect);

  if (!button->IsWindow()) {
    return;
  }

  int state = -1;
  if (!menu.IsNull() && menu.IsMenu()) {
    state = menu.GetMenuState(button_sc_id, MF_BYCOMMAND);
  }

  if (state == -1) {
    button->ShowWindow(SW_HIDE);
    return;
  }

  button->EnableWindow(!(state & (MF_GRAYED | MF_DISABLED)));
  button->SetWindowPos(nullptr, button_rect, SWP_NOZORDER | SWP_SHOWWINDOW);
  button_rect->OffsetRect(-button_rect->Width() - button_margin, 0);
}

void OwnerDrawTitleBarWindow::RecalcLayout() {
  CRect title_bar_rect;
  GetClientRect(&title_bar_rect);

  const int button_margin = title_bar_rect.Height() / 5;
  title_bar_rect.DeflateRect(button_margin, button_margin);

  const int button_height = title_bar_rect.Height();
  const int button_width = button_height;

  // Position controls from the Close button to the Minimize button.
  CRect button_rect(title_bar_rect.right - button_width, title_bar_rect.top,
                    title_bar_rect.right, title_bar_rect.bottom);

  WTL::CMenuHandle menu(::GetSystemMenu(GetParent(), false));
  UpdateButtonState(menu, SC_CLOSE, button_margin, &close_button_,
                    &button_rect);
  UpdateButtonState(menu, SC_MINIMIZE, button_margin, &minimize_button_,
                    &button_rect);
}

void OwnerDrawTitleBarWindow::MoveWindowToDragPosition(HWND hwnd,
                                                       int dx,
                                                       int dy) {
  CRect rect;
  ::GetWindowRect(hwnd, &rect);

  rect.OffsetRect(dx, dy);
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

  CRect title_bar_client_rect =
      ComputeTitleBarClientRect(parent_hwnd, title_bar_spacer_hwnd);

  // This title bar is a child window and occupies the top portion of the parent
  // dialog box window. DS_MODALFRAME and WS_BORDER are incompatible with this
  // title bar. WS_DLGFRAME is recommended as well.
  const LONG parent_style = ::GetWindowLong(parent_hwnd, GWL_STYLE);
  CHECK(!(parent_style & DS_MODALFRAME));
  CHECK(!(parent_style & WS_BORDER));
  CHECK(parent_style & WS_DLGFRAME);

  title_bar_window_.set_bk_color(bk_color);
  title_bar_window_.Create(
      parent_hwnd, title_bar_client_rect, nullptr,
      WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
}

void OwnerDrawTitleBar::RecalcLayout(HWND parent_hwnd,
                                     HWND title_bar_spacer_hwnd) {
  if (!title_bar_window_.IsWindow()) {
    return;
  }

  // Re-compute where the title bar window should be based on the spacer. The
  // spacer was already resized by the OS Dialog Manager.
  CRect new_rect =
      ComputeTitleBarClientRect(parent_hwnd, title_bar_spacer_hwnd);

  // Resize the title bar window itself.
  title_bar_window_.SetWindowPos(nullptr, &new_rect,
                                 SWP_NOZORDER | SWP_NOACTIVATE);

  // Now tell the window to reposition its internal buttons.
  title_bar_window_.RecalcLayout();
}

CRect OwnerDrawTitleBar::ComputeTitleBarClientRect(HWND parent_hwnd,
                                                   HWND title_bar_spacer_hwnd) {
  CHECK(parent_hwnd);

  CRect parent_client_rect;
  ::GetClientRect(parent_hwnd, &parent_client_rect);
  CRect title_bar_client_rect(parent_client_rect);

  CRect title_bar_spacer_client_rect;
  ::GetClientRect(title_bar_spacer_hwnd, &title_bar_spacer_client_rect);
  const int title_bar_height(title_bar_spacer_client_rect.Height());

  title_bar_client_rect.bottom = title_bar_client_rect.top + title_bar_height;

  return title_bar_client_rect;
}

CustomDlgColors::CustomDlgColors() = default;
CustomDlgColors::~CustomDlgColors() = default;

void CustomDlgColors::SetCustomDlgColors(COLORREF text_color,
                                         COLORREF bk_color) {
  text_color_ = text_color;
  bk_color_ = bk_color;

  CHECK(bk_brush_.IsNull());
  bk_brush_.CreateSolidBrush(bk_color_);
}

LRESULT CustomDlgColors::OnCtrlColor(UINT,
                                     WPARAM wparam,
                                     LPARAM,
                                     BOOL& handled) {
  handled = true;

  WTL::CDCHandle dc(reinterpret_cast<HDC>(wparam));
  SetBkColor(dc, GetColor(bk_color_, COLOR_WINDOW));
  SetTextColor(dc, GetColor(text_color_, COLOR_WINDOWTEXT));

  return reinterpret_cast<LRESULT>(GetColorBrush(bk_brush_, COLOR_WINDOW));
}

CustomProgressBarCtrl::CustomProgressBarCtrl() = default;

CustomProgressBarCtrl::~CustomProgressBarCtrl() = default;

LRESULT CustomProgressBarCtrl::OnEraseBkgnd(UINT,
                                            WPARAM,
                                            LPARAM,
                                            BOOL& handled) {
  // The background and foreground are both rendered in OnPaint().
  handled = true;
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

LRESULT CustomProgressBarCtrl::OnPaint(UINT, WPARAM, LPARAM, BOOL& handled) {
  handled = true;

  WTL::CPaintDC dc_paint(m_hWnd);
  CRect window_rect;
  GetClientRect(&window_rect);

  // Calculate a half-width rectangle.
  CRect client_rect = window_rect;
  const int original_height = window_rect.Height();
  const int slim_height = original_height / 2;
  const int vertical_padding = (original_height - slim_height) / 2;

  // Shrink the top and bottom to center the bar.
  client_rect.top += vertical_padding;
  client_rect.bottom -= vertical_padding;

  // Using a memory device context eliminates flicker.
  WTL::CMemoryDC dc(dc_paint, window_rect);

  // Draw the parent's gradient background into the memory DC first. This
  // ensures the empty areas of the progress bar show the gradient.
  DrawParentBackground(m_hWnd, dc.m_hDC, window_rect);

  // Draw at 4x scale to get smooth edges when scaling back down.
  constexpr int kScale = 4;
  CRect high_res_rect(0, 0, client_rect.Width() * kScale,
                      client_rect.Height() * kScale);

  WTL::CDC dc_hi_res;
  dc_hi_res.CreateCompatibleDC(dc.m_hDC);

  WTL::CBitmap bmp_hi_res;
  bmp_hi_res.CreateCompatibleBitmap(dc.m_hDC, high_res_rect.Width(),
                                    high_res_rect.Height());
  const HBITMAP old_bmp = dc_hi_res.SelectBitmap(bmp_hi_res);

  // Fill the high-res background with the fill color to avoid bleeding at the
  // edges.
  dc_hi_res.FillSolidRect(&high_res_rect, empty_fill_color_);

  // Setup GDI objects for rounded drawing. `NULL_PEN` prevents the thin black
  // border around the shapes.
  const HPEN old_pen =
      dc.SelectPen(static_cast<HPEN>(::GetStockObject(NULL_PEN)));
  const int corner_size = high_res_rect.Height();

  // Draw the Background Track.
  WTL::CBrush bg_brush = ::CreateSolidBrush(empty_fill_color_);
  const HBRUSH old_brush = dc.SelectBrush(bg_brush);
  dc_hi_res.RoundRect(&high_res_rect, {corner_size, corner_size});

  // Calculate Progress Width.
  const int kBarWidth = kMaxPosition - kMinPosition;
  if (kBarWidth > 0) {
    const LONG bar_rect_right =
        high_res_rect.left +
        high_res_rect.Width() * (current_position_ - kMinPosition) / kBarWidth;

    CRect progress_rect = high_res_rect;
    progress_rect.right = std::min(bar_rect_right, high_res_rect.right);

    // Handle Marquee Style animation.
    if (GetStyle() & PBS_MARQUEE) {
      const LONG bar_rect_left =
          bar_rect_right - (high_res_rect.Width() * kMarqueeWidth / kBarWidth);
      progress_rect.left = std::max(bar_rect_left, high_res_rect.left);
    }

    // Draw the fill.
    if (!progress_rect.IsRectEmpty() &&
        progress_rect.Width() > (corner_size / 2)) {
      WTL::CBrush fill_brush = ::CreateSolidBrush(bar_color_);
      dc_hi_res.SelectBrush(fill_brush);
      dc_hi_res.RoundRect(&progress_rect, {corner_size, corner_size});
    }
  }

  // `HALFTONE` creates a smooth anti-aliased look.
  ::SetStretchBltMode(dc.m_hDC, HALFTONE);

  // Required for HALFTONE to work correctly.
  ::SetBrushOrgEx(dc.m_hDC, 0, 0, NULL);

  // Scale the high-res bar back down to the screen.
  dc.StretchBlt(client_rect.left, client_rect.top, client_rect.Width(),
                client_rect.Height(), dc_hi_res.m_hDC, 0, 0,
                high_res_rect.Width(), high_res_rect.Height(), SRCCOPY);

  // Cleanup.
  dc_hi_res.SelectBrush(old_brush);
  dc_hi_res.SelectPen(old_pen);
  dc_hi_res.SelectBitmap(old_bmp);

  return 0;
}

LRESULT CustomProgressBarCtrl::OnTimer(UINT,
                                       WPARAM event_id,
                                       LPARAM,
                                       BOOL& handled) {
  handled = true;

  if (event_id != kMarqueeTimerId) {
    handled = false;
    return 0;
  }

  ::SendMessage(m_hWnd, PBM_SETPOS, 0, 0L);
  return 0;
}

LRESULT CustomProgressBarCtrl::OnSetPos(UINT,
                                        WPARAM new_position,
                                        LPARAM,
                                        BOOL& handled) {
  // To allow accessibility to show the correct progress values, we pass
  // PBM_SETPOS to the underlying Win32 control.
  handled = false;

  int old_position = current_position_;

  if (GetStyle() & PBS_MARQUEE) {
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

  RedrawWindow();

  return old_position;
}

// Calling WM_SETBARCOLOR will convert the progress bar into a solid colored
// bar. i.e., no gradient.
LRESULT CustomProgressBarCtrl::OnSetBarColor(UINT,
                                             WPARAM,
                                             LPARAM bar_color,
                                             BOOL& handled) {
  handled = true;

  COLORREF old_bar_color = bar_color_;
  bar_color_ = static_cast<COLORREF>(bar_color);

  RedrawWindow();

  return old_bar_color;
}

LRESULT CustomProgressBarCtrl::OnSetBkColor(UINT,
                                            WPARAM,
                                            LPARAM empty_fill_color,
                                            BOOL& handled) {
  handled = true;

  COLORREF old_empty_fill_color = empty_fill_color_;
  empty_fill_color_ = static_cast<COLORREF>(empty_fill_color);

  RedrawWindow();

  return old_empty_fill_color;
}

LRESULT CustomProgressBarCtrl::OnSetMarquee(UINT,
                                            WPARAM is_set_marquee,
                                            LPARAM update_msec,
                                            BOOL& handled) {
  // To allow accessibility to show the correct progress values, we pass
  // PBM_SETMARQUEE to the underlying Win32 control.
  handled = false;

  if (is_set_marquee && !is_marquee_mode_) {
    current_position_ = kMinPosition;
    SetTimer(kMarqueeTimerId, static_cast<UINT>(update_msec));
    is_marquee_mode_ = true;
  } else if (!is_set_marquee && is_marquee_mode_) {
    KillTimer(kMarqueeTimerId);
    is_marquee_mode_ = false;
  }

  RedrawWindow();

  return is_set_marquee;
}

}  // namespace updater::ui
