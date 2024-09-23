// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_USER_CREATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_USER_CREATION_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class UserCreationScreen;

// Interface for dependency injection between UserCreationScreen and its
// WebUI representation.
class UserCreationView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"user-creation",
                                                       "UserCreationScreen"};

  virtual ~UserCreationView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  virtual void SetIsBackButtonVisible(bool value) = 0;
  virtual void SetTriageStep() = 0;
  virtual void SetChildSetupStep() = 0;
  virtual void SetDefaultStep() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<UserCreationView> AsWeakPtr() = 0;
};

class UserCreationScreenHandler final : public UserCreationView,
                                        public BaseScreenHandler {
 public:
  using TView = UserCreationView;

  UserCreationScreenHandler();

  ~UserCreationScreenHandler() override;

  UserCreationScreenHandler(const UserCreationScreenHandler&) = delete;
  UserCreationScreenHandler& operator=(const UserCreationScreenHandler&) =
      delete;

 private:
  void Show() override;
  void SetIsBackButtonVisible(bool value) override;
  void SetTriageStep() override;
  void SetChildSetupStep() override;
  void SetDefaultStep() override;
  base::WeakPtr<UserCreationView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  base::WeakPtrFactory<UserCreationView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_USER_CREATION_SCREEN_HANDLER_H_
