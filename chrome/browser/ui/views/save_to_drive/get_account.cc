// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/save_to_drive/get_account.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_controller.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace save_to_drive {

AccountChooser::AccountChooser() = default;
AccountChooser::~AccountChooser() = default;

void AccountChooser::GetAccount(
    content::WebContents* web_contents,
    base::OnceCallback<void(std::optional<AccountInfo>)>
        on_account_chosen_callback) {
  account_chooser_controller_ = std::make_unique<AccountChooserController>(
      web_contents,
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext())));
  account_chooser_controller_->GetAccount(
      std::move(on_account_chosen_callback));
}

}  // namespace save_to_drive
