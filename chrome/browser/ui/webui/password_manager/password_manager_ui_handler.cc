// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/password_manager_ui_handler.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/ui/webui/password_manager/password_manager.mojom-forward.h"
#include "chrome/browser/ui/webui/password_manager/password_manager.mojom.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/actor_login_permission.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

PasswordManagerUIHandler::PasswordManagerUIHandler(
    mojo::PendingReceiver<password_manager::mojom::PageHandler> receiver,
    mojo::PendingRemote<password_manager::mojom::Page> page,
    scoped_refptr<extensions::PasswordsPrivateDelegate>
        passwords_private_delegate,
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      passwords_private_delegate_(std::move(passwords_private_delegate)),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {}

PasswordManagerUIHandler::~PasswordManagerUIHandler() = default;

void PasswordManagerUIHandler::ExtendAuthValidity() {
  passwords_private_delegate_->RestartAuthTimer();
}

void PasswordManagerUIHandler::DeleteAllPasswordManagerData(
    DeleteAllPasswordManagerDataCallback callback) {
  // TODO(crbug.com/432409279): don't use the delegate, but instead use the
  // password manager backend directly.
  passwords_private_delegate_->DeleteAllPasswordManagerData(
      web_contents_, std::move(callback));
}

void PasswordManagerUIHandler::CopyPlaintextBackupPassword(
    int id,
    CopyPlaintextBackupPasswordCallback callback) {
  passwords_private_delegate_->CopyPlaintextBackupPassword(id, web_contents_,
                                                           std::move(callback));
}

void PasswordManagerUIHandler::RemoveBackupPassword(int id) {
  passwords_private_delegate_->RemoveBackupPassword(id);
}

void PasswordManagerUIHandler::GetActorLoginPermissions(
    GetActorLoginPermissionsCallback callback) {
  std::vector<password_manager::mojom::ActorLoginPermissionPtr> result;
  for (const auto& site :
       GetSavedPasswordsPresenter()->GetActorLoginPermissions()) {
    auto url = password_manager::mojom::FormattedUrl::New(
        /*human_readable_url=*/password_manager::GetShownOrigin(
            url::Origin::Create(site.url)),
        /*link=*/site.url.spec());
    result.push_back(password_manager::mojom::ActorLoginPermission::New(
        std::move(url), base::UTF16ToUTF8(site.username)));
  }
  std::move(callback).Run(std::move(result));
}

void PasswordManagerUIHandler::RevokeActorLoginPermission(
    password_manager::mojom::ActorLoginPermissionPtr site) {
  GetSavedPasswordsPresenter()->RevokeActorLoginPermission(
      password_manager::ActorLoginPermission{
          .url = GURL(site->url->link),
          .username = base::UTF8ToUTF16(site->username)});
}

password_manager::SavedPasswordsPresenter*
PasswordManagerUIHandler::GetSavedPasswordsPresenter() {
  return passwords_private_delegate_->GetSavedPasswordsPresenter();
}
