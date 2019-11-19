// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ACCOUNT_MANAGER_WELCOME_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ACCOUNT_MANAGER_WELCOME_UI_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// For chrome:://account-manager-welcome
class AccountManagerWelcomeUI : public ui::WebDialogUI {
 public:
  explicit AccountManagerWelcomeUI(content::WebUI* web_ui);
  ~AccountManagerWelcomeUI() override;

 private:
  base::WeakPtrFactory<AccountManagerWelcomeUI> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(AccountManagerWelcomeUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ACCOUNT_MANAGER_WELCOME_UI_H_
