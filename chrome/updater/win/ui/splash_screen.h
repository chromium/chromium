// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_SPLASH_SCREEN_H_
#define CHROME_UPDATER_WIN_UI_SPLASH_SCREEN_H_

#include <windows.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "base/win/atl.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/updater/win/ui/owner_draw_controls.h"
#include "chrome/updater/win/ui/resources/resources.grh"

namespace updater {
namespace ui {

class SplashScreen : public CAxDialogImpl<SplashScreen>,
                     public OwnerDrawTitleBar,
                     public CustomDlgColors {
 public:
  static constexpr int IDD = IDD_PROGRESS;

  explicit SplashScreen(const base::string16& bundle_name);
  ~SplashScreen() override;
  void Show();

  // Does alpha blending and closese the window.
  void Dismiss(base::OnceClosure on_close_closure);

  BEGIN_MSG_MAP(SplashScreen)
    MESSAGE_HANDLER(WM_TIMER, OnTimer)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    CHAIN_MSG_MAP(OwnerDrawTitleBar)
    CHAIN_MSG_MAP(CustomDlgColors)
  END_MSG_MAP()

 private:
  enum class WindowState {
    STATE_CREATED,
    STATE_INITIALIZED,
    STATE_SHOW_NORMAL,
    STATE_FADING,
    STATE_CLOSED,
  };

  HRESULT Initialize();
  void InitProgressBar();
  void EnableSystemButtons(bool enable);
  void SwitchToState(WindowState new_state);
  void Close();

  // Message and command handlers.
  LRESULT OnTimer(UINT msg,
                  WPARAM wparam,
                  LPARAM lparam,
                  BOOL& handled);  // NOLINT
  LRESULT OnClose(UINT msg,
                  WPARAM wparam,
                  LPARAM lparam,
                  BOOL& handled);  // NOLINT
  LRESULT OnDestroy(UINT msg,
                    WPARAM wparam,
                    LPARAM lparam,
                    BOOL& handled);  // NOLINT

  THREAD_CHECKER(thread_checker_);

  // State of the object.
  WindowState state_;

  // Indicates whether timer for fading effect has been created.
  bool timer_created_;

  // Array index of current alpha blending value.
  int alpha_index_;

  // Dialog title.
  base::string16 title_;

  WTL::CFont default_font_;
  WTL::CFont font_;

  // Handle to large icon to show when ALT-TAB
  base::win::ScopedGDIObject<HICON> hicon_;
  CustomProgressBarCtrl progress_bar_;

  // Called when the window is destroyed.
  base::OnceClosure on_close_closure_;

  DISALLOW_COPY_AND_ASSIGN(SplashScreen);
};

}  // namespace ui
}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UI_SPLASH_SCREEN_H_
