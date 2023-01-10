// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_SPLASH_SCREEN_H_
#define CHROME_UPDATER_WIN_UI_SPLASH_SCREEN_H_

#include <windows.h>

#include <string>

#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "base/win/atl.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/updater/splash_screen.h"
#include "chrome/updater/win/ui/owner_draw_controls.h"
#include "chrome/updater/win/ui/resources/resources.grh"

namespace updater {
namespace ui {

class SilentSplashScreen : public updater::SplashScreen {
 public:
  // Overrides for SplashScreen.
  void Show() override;
  void Dismiss(base::OnceClosure callback) override;
};

class SplashScreen : public CAxDialogImpl<SplashScreen>,
                     public CustomDlgColors,
                     public OwnerDrawTitleBar,
                     public updater::SplashScreen {
 public:
  static constexpr int IDD = IDD_PROGRESS;

  explicit SplashScreen(const std::u16string& bundle_name);
  SplashScreen(const SplashScreen&) = delete;
  SplashScreen& operator=(const SplashScreen&) = delete;
  ~SplashScreen() override;

  // Overrides for SplashScreen.
  void Show() override;
  void Dismiss(base::OnceClosure on_close_closure) override;

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
  std::wstring title_;

  WTL::CFont default_font_;
  WTL::CFont font_;

  // Handle to large icon to show when ALT-TAB
  base::win::ScopedGDIObject<HICON> hicon_;
  CustomProgressBarCtrl progress_bar_;

  // Called when the window is destroyed.
  base::OnceClosure on_close_closure_;
};

}  // namespace ui
}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UI_SPLASH_SCREEN_H_
