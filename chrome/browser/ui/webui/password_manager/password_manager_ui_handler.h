// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PASSWORD_MANAGER_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PASSWORD_MANAGER_UI_HANDLER_H_

#include "chrome/browser/ui/webui/password_manager/password_manager.mojom-forward.h"
#include "chrome/browser/ui/webui/password_manager/password_manager.mojom.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace extensions {
class PasswordsPrivateDelegate;
}

namespace content {
class WebContents;
}

class PasswordManagerUIHandler : public password_manager::mojom::PageHandler {
 public:
  PasswordManagerUIHandler(
      mojo::PendingReceiver<password_manager::mojom::PageHandler> receiver,
      mojo::PendingRemote<password_manager::mojom::Page> page,
      scoped_refptr<extensions::PasswordsPrivateDelegate>
          passwords_private_delegate,
      content::WebContents* web_contents);

  PasswordManagerUIHandler(const PasswordManagerUIHandler&) = delete;
  PasswordManagerUIHandler& operator=(const PasswordManagerUIHandler&) = delete;

  ~PasswordManagerUIHandler() override;

  // password_manager::mojom::PageHandler:
  void ExtendAuthValidity() override;

  void DeleteAllPasswordManagerData(
      DeleteAllPasswordManagerDataCallback callback) override;

  void CopyPlaintextBackupPassword(
      int id,
      CopyPlaintextBackupPasswordCallback callback) override;

  void RemoveBackupPassword(int id) override;

  void GetActorLoginPermissions(
      GetActorLoginPermissionsCallback callback) override;

  void RevokeActorLoginPermission(
      password_manager::mojom::ActorLoginPermissionPtr site) override;

  void ChangePasswordManagerPin(
      ChangePasswordManagerPinCallback callback) override;

  void IsPasswordManagerPinAvailable(
      IsPasswordManagerPinAvailableCallback callback) override;

  void ShowAddShortcutDialog() override;

  void IsAccountStorageEnabled(
      IsAccountStorageEnabledCallback callback) override;

  void SetAccountStorageEnabled(bool enabled) override;

  void ShouldShowAccountStorageSettingToggle(
      ShouldShowAccountStorageSettingToggleCallback callback) override;

  void SwitchBiometricAuthBeforeFillingState(
      SwitchBiometricAuthBeforeFillingStateCallback callback) override;

 private:
  password_manager::SavedPasswordsPresenter* GetSavedPasswordsPresenter();

  raw_ptr<content::WebContents> web_contents_;
  scoped_refptr<extensions::PasswordsPrivateDelegate>
      passwords_private_delegate_;

  // NOTE: These are located at the end of the list of member variables to
  // ensure the WebUI page is disconnected before other members are destroyed.
  mojo::Receiver<password_manager::mojom::PageHandler> receiver_;
  mojo::Remote<password_manager::mojom::Page> page_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PASSWORD_MANAGER_UI_HANDLER_H_
