// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_USER_ALLOWLIST_CHECK_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_USER_ALLOWLIST_CHECK_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class UserAllowlistCheckScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "user-allowlist-check-screen", "UserAllowlistCheckScreen"};

  virtual void Show(bool enterprise_managed, bool family_link_allowed) = 0;
  virtual void Hide() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<UserAllowlistCheckScreenView> AsWeakPtr() = 0;
};

// A class that handles WebUI hooks in Gaia screen.
class UserAllowlistCheckScreenHandler final
    : public UserAllowlistCheckScreenView,
      public BaseScreenHandler {
 public:
  using TView = UserAllowlistCheckScreenView;

  UserAllowlistCheckScreenHandler();

  UserAllowlistCheckScreenHandler(const UserAllowlistCheckScreenHandler&) =
      delete;
  UserAllowlistCheckScreenHandler& operator=(
      const UserAllowlistCheckScreenHandler&) = delete;

  ~UserAllowlistCheckScreenHandler() override;

  void Show(bool enterprise_managed, bool family_link_allowed) override;
  void Hide() override;
  base::WeakPtr<UserAllowlistCheckScreenView> AsWeakPtr() override;

 private:
  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;

  base::WeakPtrFactory<UserAllowlistCheckScreenHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_USER_ALLOWLIST_CHECK_SCREEN_HANDLER_H_
