// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_OWNER_DRAW_CONTROLS_H_
#define CHROME_UPDATER_WIN_UI_OWNER_DRAW_CONTROLS_H_

#include <windows.h>

#include "base/macros.h"
#include "base/win/atl.h"
#include "chrome/updater/win/ui/constants.h"

// These headers must be included after base/win/atl.h.
#include "./atlapp.h"
#include "./atlctrls.h"
#include "./atlframe.h"
#include "./atltypes.h"

namespace updater {
namespace ui {

class CaptionButton : public CWindowImpl<CaptionButton, WTL::CButton>,
                      public WTL::COwnerDraw<CaptionButton> {
 public:
  DECLARE_WND_CLASS_EX(_T("CaptionButton"),
                       CS_HREDRAW | CS_VREDRAW,
                       COLOR_WINDOW)
  CaptionButton();
  ~CaptionButton() override;

  void DrawItem(LPDRAWITEMSTRUCT draw_item_struct);

  COLORREF bk_color() const;
  void set_bk_color(COLORREF bk_color);

  CString tool_tip_text() const;
  void set_tool_tip_text(const CString& tool_tip_text);

  BEGIN_MSG_MAP(CaptionButton)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MESSAGE_RANGE_HANDLER(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseMessage)
    MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
    MESSAGE_HANDLER(WM_MOUSEHOVER, OnMouseHover)
    MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
    MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
    CHAIN_MSG_MAP_ALT(COwnerDraw<CaptionButton>, 1)
    DEFAULT_REFLECTION_HANDLER()
  END_MSG_MAP()

 private:
  virtual HRGN GetButtonRgn(int rgn_width, int rgn_height) = 0;

  LRESULT OnCreate(UINT msg,
                   WPARAM wparam,
                   LPARAM lparam,
                   BOOL& handled);  // NOLINT
  LRESULT OnMouseMessage(UINT msg,
                         WPARAM wparam,
                         LPARAM lparam,
                         BOOL& handled);  // NOLINT
  LRESULT OnEraseBkgnd(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  LRESULT OnMouseMove(UINT msg,
                      WPARAM wparam,
                      LPARAM lparam,
                      BOOL& handled);  // NOLINT
  LRESULT OnMouseHover(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  LRESULT OnMouseLeave(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT

  COLORREF bk_color_ = RGB(0, 0, 0);
  WTL::CBrush foreground_brush_ = ::CreateSolidBrush(kCaptionForegroundColor);
  WTL::CBrush frame_brush_ = ::CreateSolidBrush(kCaptionFrameColor);

  WTL::CToolTipCtrl tool_tip_window_;
  CString tool_tip_text_;
  bool is_tracking_mouse_events_ = false;
  bool is_mouse_hovering_ = false;

  DISALLOW_COPY_AND_ASSIGN(CaptionButton);
};

class CloseButton : public CaptionButton {
 public:
  CloseButton();

 private:
  HRGN GetButtonRgn(int rgn_width, int rgn_height) override;

  DISALLOW_COPY_AND_ASSIGN(CloseButton);
};

class MinimizeButton : public CaptionButton {
 public:
  MinimizeButton();

 private:
  HRGN GetButtonRgn(int rgn_width, int rgn_height) override;

  DISALLOW_COPY_AND_ASSIGN(MinimizeButton);
};

class MaximizeButton : public CaptionButton {
 public:
  MaximizeButton();

 private:
  HRGN GetButtonRgn(int rgn_width, int rgn_height) override;

  DISALLOW_COPY_AND_ASSIGN(MaximizeButton);
};

class OwnerDrawTitleBarWindow : public CWindowImpl<OwnerDrawTitleBarWindow> {
 public:
  enum ButtonIds {
    kButtonClose = 1,
    kButtonMaximize,
    kButtonMinimize,
  };

  DECLARE_WND_CLASS_EX(_T("OwnerDrawTitleBarWindow"),
                       CS_HREDRAW | CS_VREDRAW,
                       COLOR_WINDOW)

  BEGIN_MSG_MAP(OwnerDrawTitleBarWindow)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
    MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
    MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
    MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
    COMMAND_ID_HANDLER(kButtonClose, OnClose)
    COMMAND_ID_HANDLER(kButtonMaximize, OnMaximize)
    COMMAND_ID_HANDLER(kButtonMinimize, OnMinimize)
    REFLECT_NOTIFICATIONS()
  END_MSG_MAP()

  OwnerDrawTitleBarWindow();
  ~OwnerDrawTitleBarWindow() override;

  void RecalcLayout();

  COLORREF bk_color() const;
  void set_bk_color(COLORREF bk_color);

 private:
  void CreateCaptionButtons();
  void UpdateButtonState(const WTL::CMenuHandle& menu,
                         UINT button_sc_id,
                         const int button_margin,
                         CaptionButton* button,
                         CRect* button_rect);
  void MoveWindowToDragPosition(HWND hwnd, int dx, int dy);

  LRESULT OnCreate(UINT msg,
                   WPARAM wparam,
                   LPARAM lparam,
                   BOOL& handled);  // NOLINT
  LRESULT OnDestroy(UINT msg,
                    WPARAM wparam,
                    LPARAM lparam,
                    BOOL& handled);  // NOLINT
  LRESULT OnMouseMove(UINT msg,
                      WPARAM wparam,
                      LPARAM lparam,
                      BOOL& handled);  // NOLINT
  LRESULT OnLButtonDown(UINT msg,
                        WPARAM wparam,
                        LPARAM lparam,
                        BOOL& handled);  // NOLINT
  LRESULT OnLButtonUp(UINT msg,
                      WPARAM wparam,
                      LPARAM lparam,
                      BOOL& handled);  // NOLINT
  LRESULT OnEraseBkgnd(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  LRESULT OnClose(WORD notify_code,
                  WORD id,
                  HWND hwnd_ctrl,
                  BOOL& handled);  // NOLINT
  LRESULT OnMaximize(WORD notify_code,
                     WORD id,
                     HWND hwnd_ctrl,
                     BOOL& handled);  // NOLINT
  LRESULT OnMinimize(WORD notify_code,
                     WORD id,
                     HWND hwnd_ctrl,
                     BOOL& handled);  // NOLINT

  CPoint current_drag_position_ = {-1, -1};
  COLORREF bk_color_ = RGB(0, 0, 0);

  CloseButton close_button_;
  MinimizeButton minimize_button_;

  DISALLOW_COPY_AND_ASSIGN(OwnerDrawTitleBarWindow);
};

class OwnerDrawTitleBar {
 public:
  OwnerDrawTitleBar();
  ~OwnerDrawTitleBar();

  void CreateOwnerDrawTitleBar(HWND parent_hwnd,
                               HWND title_bar_spacer_hwnd,
                               COLORREF bk_color);

  void RecalcLayout();

  BEGIN_MSG_MAP(OwnerDrawTitleBar)
  END_MSG_MAP()

 private:
  CRect ComputeTitleBarClientRect(HWND parent_hwnd, HWND title_bar_spacer_hwnd);

  OwnerDrawTitleBarWindow title_bar_window_;

  DISALLOW_COPY_AND_ASSIGN(OwnerDrawTitleBar);
};

// Customizes the text color and the background color for dialog elements.
//
// Steps:
// - Derive your ATL dialog class from CustomDlgColors.
// - Add a CHAIN_MSG_MAP in your ATL message map to CustomDlgColors.
//   CustomDlgColors will handle WM_CTLCOLOR{XXX} in the chained message map.
// - Call SetCustomDlgColors() from OnInitDialog.
class CustomDlgColors {
 public:
  CustomDlgColors();
  ~CustomDlgColors();

  void SetCustomDlgColors(COLORREF text_color, COLORREF bk_color);

  BEGIN_MSG_MAP(CustomDlgColors)
    MESSAGE_HANDLER(WM_CTLCOLORDLG, OnCtrlColor)
    MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtrlColor)
  END_MSG_MAP()

 private:
  LRESULT OnCtrlColor(UINT msg,
                      WPARAM wparam,
                      LPARAM lparam,
                      BOOL& handled);  // NOLINT

  COLORREF text_color_ = RGB(0xFF, 0xFF, 0xFF);
  COLORREF bk_color_ = RGB(0, 0, 0);
  WTL::CBrush bk_brush_;

  DISALLOW_COPY_AND_ASSIGN(CustomDlgColors);
};

class CustomProgressBarCtrl : public CWindowImpl<CustomProgressBarCtrl> {
 public:
  CustomProgressBarCtrl();
  ~CustomProgressBarCtrl() override;

  BEGIN_MSG_MAP(CustomProgressBarCtrl)
    MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
    MESSAGE_HANDLER(WM_PAINT, OnPaint)
    MESSAGE_HANDLER(WM_TIMER, OnTimer)
    MESSAGE_HANDLER(PBM_SETPOS, OnSetPos)
    MESSAGE_HANDLER(PBM_SETMARQUEE, OnSetMarquee)
    MESSAGE_HANDLER(PBM_SETBARCOLOR, OnSetBarColor)
    MESSAGE_HANDLER(PBM_SETBKCOLOR, OnSetBkColor)
  END_MSG_MAP()

 private:
  void GradientFill(HDC dc,
                    const RECT& rect,
                    COLORREF top_color,
                    COLORREF bottom_color);

  LRESULT OnEraseBkgnd(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  LRESULT OnPaint(UINT msg,
                  WPARAM wparam,
                  LPARAM lparam,
                  BOOL& handled);  // NOLINT
  LRESULT OnTimer(UINT msg,
                  WPARAM wparam,
                  LPARAM lparam,
                  BOOL& handled);  // NOLINT

  LRESULT OnSetPos(UINT msg,
                   WPARAM wparam,
                   LPARAM lparam,
                   BOOL& handled);  // NOLINT
  LRESULT OnSetMarquee(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  LRESULT OnSetBarColor(UINT msg,
                        WPARAM wparam,
                        LPARAM lparam,
                        BOOL& handled);  // NOLINT
  LRESULT OnSetBkColor(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT

  static constexpr int kMinPosition = 0;
  static constexpr int kMaxPosition = 100;
  static constexpr int kMarqueeWidth = 20;
  static constexpr UINT_PTR kMarqueeTimerId = 111;

  bool is_marquee_mode_ = false;
  int current_position_ = kMinPosition;

  COLORREF bar_color_light_ = kProgressBarLightColor;
  COLORREF bar_color_dark_ = kProgressBarDarkColor;
  COLORREF empty_fill_color_ = kProgressEmptyFillColor;
  WTL::CBrush empty_frame_brush_;

  DISALLOW_COPY_AND_ASSIGN(CustomProgressBarCtrl);
};

}  // namespace ui
}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UI_OWNER_DRAW_CONTROLS_H_
