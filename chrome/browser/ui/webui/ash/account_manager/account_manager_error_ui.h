// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_ERROR_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_ERROR_UI_H_

#include "base/memory/weak_ptr.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {

// For chrome:://account-manager-error
class AccountManagerErrorUI : public ui::WebDialogUI {
 public:
  explicit AccountManagerErrorUI(content::WebUI* web_ui);
  AccountManagerErrorUI(const AccountManagerErrorUI&) = delete;
  AccountManagerErrorUI& operator=(const AccountManagerErrorUI&) = delete;
  ~AccountManagerErrorUI() override;

 private:
  base::WeakPtrFactory<AccountManagerErrorUI> weak_factory_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_ERROR_UI_H_
