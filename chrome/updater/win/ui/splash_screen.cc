// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/splash_screen.h"

#include <cstdint>
#include <utility>

#include "base/stl_util.h"
#include "chrome/updater/win/ui/constants.h"
#include "chrome/updater/win/ui/ui.h"
#include "chrome/updater/win/ui/util.h"

namespace updater {
namespace ui {

namespace {

constexpr int kClosingTimerID = 1;

// Frequency of alpha blending value changes during window fading state.
constexpr int kTimerInterval = 100;

// Alpha blending values for the fading effect.
constexpr int kDefaultAlphaScale = 100;
constexpr int kAlphaScales[] = {0, 30, 47, 62, 75, 85, 93, kDefaultAlphaScale};

uint8_t AlphaScaleToAlphaValue(int alpha_scale) {
  DCHECK(alpha_scale >= 0 && alpha_scale <= 100);
  return static_cast<uint8_t>(alpha_scale * 255 / 100);
}

}  // namespace

SplashScreen::SplashScreen(const base::string16& bundle_name)
    : timer_created_(false), alpha_index_(0) {
  title_ = GetInstallerDisplayName(bundle_name);
  SwitchToState(WindowState::STATE_CREATED);
}

SplashScreen::~SplashScreen() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(state_ == WindowState::STATE_CREATED ||
         state_ == WindowState::STATE_CLOSED);
}

void SplashScreen::Show() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(WindowState::STATE_CREATED, state_);

  if (FAILED(Initialize()))
    return;

  DCHECK(IsWindow());
  ShowWindow(SW_SHOWNORMAL);
  SwitchToState(WindowState::STATE_SHOW_NORMAL);
}

void SplashScreen::Dismiss(base::OnceClosure on_close_closure) {
  on_close_closure_ = std::move(on_close_closure);
  switch (state_) {
    case WindowState::STATE_CREATED:
      SwitchToState(WindowState::STATE_CLOSED);
      break;

    case WindowState::STATE_SHOW_NORMAL:
      SwitchToState(WindowState::STATE_FADING);
      break;

    case WindowState::STATE_CLOSED:
    case WindowState::STATE_FADING:
    case WindowState::STATE_INITIALIZED:
      break;

    default:
      DCHECK(false);
      break;
  }
}

HRESULT SplashScreen::Initialize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!IsWindow());
  DCHECK(state_ == WindowState::STATE_CREATED);

  if (!Create(nullptr))
    return E_FAIL;

  HideWindowChildren(*this);

  SetWindowText(title_.c_str());

  EnableSystemButtons(false);

  base::string16 text;
  LoadString(IDS_SPLASH_SCREEN_MESSAGE, &text);
  CWindow text_wnd = GetDlgItem(IDC_INSTALLER_STATE_TEXT);
  text_wnd.SetWindowText(text.c_str());
  text_wnd.ShowWindow(SW_SHOWNORMAL);

  InitProgressBar();

  ::SetLayeredWindowAttributes(
      m_hWnd, 0, AlphaScaleToAlphaValue(kDefaultAlphaScale), LWA_ALPHA);

  CenterWindow(nullptr);
  HRESULT hr = ui::SetWindowIcon(
      m_hWnd, IDI_APP,
      base::win::ScopedGDIObject<HICON>::Receiver(hicon_).get());
  if (FAILED(hr))
    VLOG(1) << "SetWindowIcon failed " << hr;

  default_font_.CreatePointFont(90, kDialogFont);
  SendMessageToDescendants(
      WM_SETFONT, reinterpret_cast<WPARAM>(static_cast<HFONT>(default_font_)),
      0);

  font_.CreatePointFont(150, kDialogFont);
  GetDlgItem(IDC_INSTALLER_STATE_TEXT).SetFont(font_);
  GetDlgItem(IDC_INFO_TEXT).SetFont(font_);
  GetDlgItem(IDC_COMPLETE_TEXT).SetFont(font_);
  GetDlgItem(IDC_ERROR_TEXT).SetFont(font_);

  CreateOwnerDrawTitleBar(m_hWnd, GetDlgItem(IDC_TITLE_BAR_SPACER), kBkColor);
  SetCustomDlgColors(kTextColor, kBkColor);

  EnableFlatButtons(m_hWnd);

  SwitchToState(WindowState::STATE_INITIALIZED);
  return S_OK;
}

void SplashScreen::EnableSystemButtons(bool enable) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  constexpr LONG kSysStyleMask = WS_MINIMIZEBOX | WS_SYSMENU | WS_MAXIMIZEBOX;

  if (enable) {
    SetWindowLong(GWL_STYLE, GetWindowLong(GWL_STYLE) | kSysStyleMask);
  } else {
    SetWindowLong(GWL_STYLE, GetWindowLong(GWL_STYLE) & ~kSysStyleMask);

    // Remove Close/Minimize/Maximize from the system menu.
    HMENU menu(::GetSystemMenu(*this, false));
    DCHECK(menu);
    ::RemoveMenu(menu, SC_CLOSE, MF_BYCOMMAND);
    ::RemoveMenu(menu, SC_MINIMIZE, MF_BYCOMMAND);
    ::RemoveMenu(menu, SC_MAXIMIZE, MF_BYCOMMAND);
  }
}

void SplashScreen::InitProgressBar() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  progress_bar_.SubclassWindow(GetDlgItem(IDC_PROGRESS));

  LONG_PTR style = progress_bar_.GetWindowLongPtr(GWL_STYLE);
  style |= PBS_MARQUEE | WS_CHILD | WS_VISIBLE;
  progress_bar_.SetWindowLongPtr(GWL_STYLE, style);
  progress_bar_.SendMessage(PBM_SETMARQUEE, true, 0);
}

LRESULT SplashScreen::OnTimer(UINT, WPARAM, LPARAM, BOOL& handled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(state_ == WindowState::STATE_FADING);
  DCHECK_GT(alpha_index_, 0);
  if (--alpha_index_) {
    ::SetLayeredWindowAttributes(
        m_hWnd, 0, AlphaScaleToAlphaValue(kAlphaScales[alpha_index_]),
        LWA_ALPHA);
  } else {
    Close();
  }
  handled = true;
  return 0;
}

LRESULT SplashScreen::OnClose(UINT, WPARAM, LPARAM, BOOL& handled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SwitchToState(WindowState::STATE_CLOSED);
  DestroyWindow();
  handled = true;
  return 0;
}

LRESULT SplashScreen::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (timer_created_) {
    DCHECK(IsWindow());
    KillTimer(kClosingTimerID);
  }
  std::move(on_close_closure_).Run();
  handled = true;
  return 0;
}

void SplashScreen::SwitchToState(WindowState new_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  state_ = new_state;
  switch (new_state) {
    case WindowState::STATE_CREATED:
    case WindowState::STATE_INITIALIZED:
      break;
    case WindowState::STATE_SHOW_NORMAL:
      alpha_index_ = base::size(kAlphaScales) - 1;
      break;
    case WindowState::STATE_FADING:
      DCHECK(IsWindow());
      timer_created_ = SetTimer(kClosingTimerID, kTimerInterval, nullptr) != 0;
      if (!timer_created_)
        Close();
      break;
    case WindowState::STATE_CLOSED:
      break;
  }
}

void SplashScreen::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (state_ != WindowState::STATE_CLOSED && IsWindow())
    PostMessage(WM_CLOSE, 0, 0);
}

}  // namespace ui
}  // namespace updater
