// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_USER_BOARD_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_USER_BOARD_SCREEN_HANDLER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/ui/views/user_board_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

// TODO(jdufault): Rename to UserSelectionScreenHandler and ensure this in the
// right directory. See crbug.com/672142.

// A class that handles WebUI hooks in Gaia screen.
class UserBoardScreenHandler : public BaseScreenHandler, public UserBoardView {
 public:
  using TView = UserBoardView;

  explicit UserBoardScreenHandler(JSCallsContainer* js_calls_container);
  ~UserBoardScreenHandler() override;

 private:
  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;
  void Initialize() override;

  // Handlers
  void HandleHardlockPod(const AccountId& account_id);
  void HandleAttemptUnlock(const AccountId& account_id);

  // UserBoardView implementation:
  void SetPublicSessionDisplayName(const AccountId& account_id,
                                   const std::string& display_name) override;
  void SetPublicSessionLocales(const AccountId& account_id,
                               std::unique_ptr<base::ListValue> locales,
                               const std::string& default_locale,
                               bool multiple_recommended_locales) override;
  void SetPublicSessionShowFullManagementDisclosure(
      bool show_full_management_disclosure) override;
  void ShowBannerMessage(const base::string16& message,
                         bool is_warning) override;
  void ShowUserPodCustomIcon(
      const AccountId& account_id,
      const proximity_auth::ScreenlockBridge::UserPodCustomIconOptions&
          icon_options) override;
  void HideUserPodCustomIcon(const AccountId& account_id) override;
  void SetAuthType(const AccountId& account_id,
                   proximity_auth::mojom::AuthType auth_type,
                   const base::string16& initial_value) override;
  void Bind(UserSelectionScreen* screen) override;
  void Unbind() override;
  base::WeakPtr<UserBoardView> GetWeakPtr() override;

  UserSelectionScreen* screen_ = nullptr;
  base::WeakPtrFactory<UserBoardScreenHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UserBoardScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_USER_BOARD_SCREEN_HANDLER_H_
