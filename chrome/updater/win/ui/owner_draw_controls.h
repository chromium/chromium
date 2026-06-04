// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_OWNER_DRAW_CONTROLS_H_
#define CHROME_UPDATER_WIN_UI_OWNER_DRAW_CONTROLS_H_

#include <windows.h>

#include <commctrl.h>

#include <string>

#include "base/win/scoped_gdi_object.h"
#include "chrome/updater/win/ui/ui_constants.h"
#include "chrome/updater/win/ui/window_impl.h"
#include "ui/gfx/win/msg_util.h"
#include "ui/gfx/win/window_impl.h"

namespace updater::ui {

// Owner-drawn caption button used by the custom title bar. The control is a
// real `BUTTON`-class window so it participates in MSAA/UIA as
// `ROLE_SYSTEM_PUSHBUTTON`, receives keyboard activation (Space) when
// focused, and dispatches `WM_COMMAND`/`BN_CLICKED` to its parent natively.
// The `BS_OWNERDRAW` style suppresses the default `BUTTON` paint (including
// the focus rectangle and default-button frame); the parent's `WM_DRAWITEM`
// handler routes back to `DrawItem` to paint the custom icon.
class CaptionButton : public SubclassedWindow {
 public:
  CaptionButton();
  CaptionButton(const CaptionButton&) = delete;
  CaptionButton& operator=(const CaptionButton&) = delete;
  ~CaptionButton() override;

  HWND Create(HWND parent, const RECT& bounds, int control_id);

  void DrawItem(LPDRAWITEMSTRUCT draw_item_struct);

  COLORREF bk_color() const;
  void set_bk_color(COLORREF bk_color);

  const std::wstring& tool_tip_text() const;
  void set_tool_tip_text(const std::wstring& tool_tip_text);

  CR_BEGIN_MSG_MAP_EX(CaptionButton)
    CR_MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseMessage)
    CR_MESSAGE_HANDLER_EX(WM_MOUSEMOVE, OnMouseMove)
    CR_MESSAGE_HANDLER_EX(WM_MOUSEHOVER, OnMouseHover)
    CR_MESSAGE_HANDLER_EX(WM_MOUSELEAVE, OnMouseLeave)
  CR_END_MSG_MAP()

 private:
  virtual HRGN GetButtonRgn(int rgn_width, int rgn_height) = 0;

  LRESULT OnMouseMessage(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnMouseMove(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnMouseHover(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnMouseLeave(UINT msg, WPARAM wparam, LPARAM lparam);

  COLORREF bk_color_ = RGB(0, 0, 0);
  base::win::ScopedGDIObject<HBRUSH> foreground_brush_;

  HWND tool_tip_window_ = nullptr;
  std::wstring tool_tip_text_;
  bool is_tracking_mouse_events_ = false;
  bool is_mouse_hovering_ = false;

  CR_MSG_MAP_CLASS_DECLARATIONS(CaptionButton)
};

class CloseButton : public CaptionButton {
 public:
  CloseButton();
  CloseButton(const CloseButton&) = delete;
  CloseButton& operator=(const CloseButton&) = delete;

 private:
  HRGN GetButtonRgn(int rgn_width, int rgn_height) override;
};

class MinimizeButton : public CaptionButton {
 public:
  MinimizeButton();
  MinimizeButton(const MinimizeButton&) = delete;
  MinimizeButton& operator=(const MinimizeButton&) = delete;

 private:
  HRGN GetButtonRgn(int rgn_width, int rgn_height) override;
};

class MaximizeButton : public CaptionButton {
 public:
  MaximizeButton();
  MaximizeButton(const MaximizeButton&) = delete;
  MaximizeButton& operator=(const MaximizeButton&) = delete;

 private:
  HRGN GetButtonRgn(int rgn_width, int rgn_height) override;
};

// Owner-drawn custom title bar. A child of the host dialog; positions and
// paints its caption buttons.
class OwnerDrawTitleBarWindow : public gfx::WindowImpl {
 public:
  enum ButtonIds {
    kButtonClose = 1,
    kButtonMaximize,
    kButtonMinimize,
  };

  OwnerDrawTitleBarWindow();
  OwnerDrawTitleBarWindow(const OwnerDrawTitleBarWindow&) = delete;
  OwnerDrawTitleBarWindow& operator=(const OwnerDrawTitleBarWindow&) = delete;
  ~OwnerDrawTitleBarWindow() override;

  HWND Create(HWND parent, const RECT& bounds);

  BOOL IsWindow() const { return hwnd() && ::IsWindow(hwnd()); }

  void RecalcLayout();

  COLORREF bk_color() const;
  void set_bk_color(COLORREF bk_color);

  CR_BEGIN_MSG_MAP_EX(OwnerDrawTitleBarWindow)
    CR_MESSAGE_HANDLER_EX(WM_CREATE, OnCreate)
    CR_MESSAGE_HANDLER_EX(WM_DESTROY, OnDestroy)
    CR_MESSAGE_HANDLER_EX(WM_MOUSEMOVE, OnMouseMove)
    CR_MESSAGE_HANDLER_EX(WM_LBUTTONDOWN, OnLButtonDown)
    CR_MESSAGE_HANDLER_EX(WM_LBUTTONUP, OnLButtonUp)
    CR_MESSAGE_HANDLER_EX(WM_ERASEBKGND, OnEraseBkgnd)
    CR_MESSAGE_HANDLER_EX(WM_SIZE, OnSize)
    CR_MESSAGE_HANDLER_EX(WM_DRAWITEM, OnDrawItem)
    CR_COMMAND_ID_HANDLER_EX(kButtonClose, OnClose)
    CR_COMMAND_ID_HANDLER_EX(kButtonMaximize, OnMaximize)
    CR_COMMAND_ID_HANDLER_EX(kButtonMinimize, OnMinimize)
  CR_END_MSG_MAP()

 private:
  void CreateCaptionButtons();
  void UpdateButtonState(HMENU menu,
                         UINT button_sc_id,
                         const int button_margin,
                         CaptionButton* button,
                         RECT* button_rect);
  void MoveWindowToDragPosition(HWND hwnd, int dx, int dy);

  LRESULT OnCreate(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnDestroy(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnMouseMove(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnLButtonDown(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnLButtonUp(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnEraseBkgnd(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnSize(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnDrawItem(UINT msg, WPARAM wparam, LPARAM lparam);
  void OnClose(UINT notify_code, int id, HWND ctl);
  void OnMaximize(UINT notify_code, int id, HWND ctl);
  void OnMinimize(UINT notify_code, int id, HWND ctl);

  POINT current_drag_position_ = {-1, -1};
  COLORREF bk_color_ = RGB(0, 0, 0);

  CloseButton close_button_;
  MinimizeButton minimize_button_;

  CR_MSG_MAP_CLASS_DECLARATIONS(OwnerDrawTitleBarWindow)
};

// Mixin attaching a custom title bar to a host dialog. Owns the
// `OwnerDrawTitleBarWindow` child. Provides a no-op `ProcessWindowMessage` so
// callers can chain into it with `CR_CHAIN_MSG_MAP`.
class OwnerDrawTitleBar {
 public:
  OwnerDrawTitleBar();
  OwnerDrawTitleBar(const OwnerDrawTitleBar&) = delete;
  OwnerDrawTitleBar& operator=(const OwnerDrawTitleBar&) = delete;
  ~OwnerDrawTitleBar();

  void CreateOwnerDrawTitleBar(HWND parent_hwnd,
                               HWND title_bar_spacer_hwnd,
                               COLORREF bk_color);

  void RecalcLayout(HWND parent_hwnd, HWND title_bar_spacer_hwnd);

  // No messages of its own; provided so callers can chain unconditionally.
  BOOL ProcessWindowMessage(HWND, UINT, WPARAM, LPARAM, LRESULT&, DWORD = 0) {
    return FALSE;
  }

 private:
  RECT ComputeTitleBarClientRect(HWND parent_hwnd, HWND title_bar_spacer_hwnd);

  OwnerDrawTitleBarWindow title_bar_window_;
};

// Mixin that customizes the text/background colors for dialog elements. Hosts
// chain into this via `CR_CHAIN_MSG_MAP(CustomDlgColors)` and call
// `SetCustomDlgColors()` from `WM_INITDIALOG`.
class CustomDlgColors {
 public:
  CustomDlgColors();
  CustomDlgColors(const CustomDlgColors&) = delete;
  CustomDlgColors& operator=(const CustomDlgColors&) = delete;
  ~CustomDlgColors();

  void SetCustomDlgColors(COLORREF text_color, COLORREF bk_color);

  BOOL ProcessWindowMessage(HWND hwnd,
                            UINT msg,
                            WPARAM wparam,
                            LPARAM lparam,
                            LRESULT& result,
                            DWORD msg_map_id = 0);

 private:
  COLORREF text_color_ = RGB(0xFF, 0xFF, 0xFF);
  COLORREF bk_color_ = RGB(0, 0, 0);
  base::win::ScopedGDIObject<HBRUSH> bk_brush_;
  base::win::ScopedGDIObject<HBRUSH> dark_bk_brush_;
};

// Subclassed (via `SetWindowSubclass`) progress bar control providing a
// custom look while preserving accessibility behavior of the underlying
// Win32 progress bar.
class CustomProgressBarCtrl : public SubclassedWindow {
 public:
  CustomProgressBarCtrl();
  CustomProgressBarCtrl(const CustomProgressBarCtrl&) = delete;
  CustomProgressBarCtrl& operator=(const CustomProgressBarCtrl&) = delete;
  ~CustomProgressBarCtrl() override;

  CR_BEGIN_MSG_MAP_EX(CustomProgressBarCtrl)
    CR_MESSAGE_HANDLER_EX(WM_ERASEBKGND, OnEraseBkgnd)
    CR_MESSAGE_HANDLER_EX(WM_PAINT, OnPaint)
    CR_MESSAGE_HANDLER_EX(WM_TIMER, OnTimer)
    CR_MESSAGE_HANDLER_EX(WM_SYSCOLORCHANGE, OnSysColorChange)
    CR_MESSAGE_HANDLER_EX(PBM_SETPOS, OnSetPos)
    CR_MESSAGE_HANDLER_EX(PBM_SETMARQUEE, OnSetMarquee)
    CR_MESSAGE_HANDLER_EX(PBM_SETBARCOLOR, OnSetBarColor)
    CR_MESSAGE_HANDLER_EX(PBM_SETBKCOLOR, OnSetBkColor)
  CR_END_MSG_MAP()

 private:
  void GradientFill(HDC dc,
                    const RECT& rect,
                    COLORREF top_color,
                    COLORREF bottom_color);

  LRESULT OnEraseBkgnd(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnPaint(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnTimer(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnSysColorChange(UINT msg, WPARAM wparam, LPARAM lparam);

  LRESULT OnSetPos(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnSetMarquee(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnSetBarColor(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnSetBkColor(UINT msg, WPARAM wparam, LPARAM lparam);

  static constexpr int kMinPosition = 0;
  static constexpr int kMaxPosition = 100;
  static constexpr int kMarqueeWidth = 20;
  static constexpr UINT_PTR kMarqueeTimerId = 111;

  bool is_marquee_mode_ = false;
  int current_position_ = kMinPosition;

  COLORREF bar_color_ = kProgressBarFillColor;
  COLORREF empty_fill_color_ = kProgressEmptyFillColor;

  CR_MSG_MAP_CLASS_DECLARATIONS(CustomProgressBarCtrl)
};

// A flat implementation of button subclassed from standard Win32 push buttons,
// styling them as Chrome/Google Design System (GDS) primary/secondary style
// flat buttons.
class FlatButton : public SubclassedWindow {
 public:
  FlatButton();
  FlatButton(const FlatButton&) = delete;
  FlatButton& operator=(const FlatButton&) = delete;
  ~FlatButton() override;

  CR_BEGIN_MSG_MAP_EX(FlatButton)
    CR_MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseMessage)
    CR_MESSAGE_HANDLER_EX(WM_MOUSEMOVE, OnMouseMove)
    CR_MESSAGE_HANDLER_EX(WM_MOUSEHOVER, OnMouseHover)
    CR_MESSAGE_HANDLER_EX(WM_MOUSELEAVE, OnMouseLeave)
    CR_MESSAGE_HANDLER_EX(WM_PAINT, OnPaint)
    CR_MESSAGE_HANDLER_EX(WM_ERASEBKGND, OnEraseBkgnd)
  CR_END_MSG_MAP()

  void SetIsPrimary(bool is_primary);

 private:
  LRESULT OnMouseMessage(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnMouseMove(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnMouseHover(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnMouseLeave(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnPaint(UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT OnEraseBkgnd(UINT msg, WPARAM wparam, LPARAM lparam);

  bool is_tracking_mouse_events_ = false;
  bool is_mouse_hovering_ = false;
  bool is_primary_ = true;

  CR_MSG_MAP_CLASS_DECLARATIONS(FlatButton)
};

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_OWNER_DRAW_CONTROLS_H_
