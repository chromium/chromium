// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENABLE_ADB_SIDELOADING_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENABLE_ADB_SIDELOADING_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface between enable adb sideloading screen and its representation.
class EnableAdbSideloadingScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "adb-sideloading", "EnableAdbSideloadingScreen"};

  // The constants need to be synced with oobe_adb_sideloading_screen.js.
  enum class UIState {
    UI_STATE_ERROR = 1,
    UI_STATE_SETUP = 2,
  };

  virtual ~EnableAdbSideloadingScreenView() = default;

  virtual void Show() = 0;
  virtual void SetScreenState(UIState value) = 0;
  virtual base::WeakPtr<EnableAdbSideloadingScreenView> AsWeakPtr() = 0;
};

// WebUI implementation of EnableAdbSideloadingScreenView.
class EnableAdbSideloadingScreenHandler final
    : public EnableAdbSideloadingScreenView,
      public BaseScreenHandler {
 public:
  using TView = EnableAdbSideloadingScreenView;

  EnableAdbSideloadingScreenHandler();

  EnableAdbSideloadingScreenHandler(const EnableAdbSideloadingScreenHandler&) =
      delete;
  EnableAdbSideloadingScreenHandler& operator=(
      const EnableAdbSideloadingScreenHandler&) = delete;

  ~EnableAdbSideloadingScreenHandler() override;

  // EnableAdbSideloadingScreenView implementation:
  void Show() override;
  void SetScreenState(UIState value) override;
  base::WeakPtr<EnableAdbSideloadingScreenView> AsWeakPtr() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

 private:
  base::WeakPtrFactory<EnableAdbSideloadingScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENABLE_ADB_SIDELOADING_SCREEN_HANDLER_H_
