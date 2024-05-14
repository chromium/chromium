// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/password_manager_handler.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"

namespace settings {

namespace {

// WARNING: Keep synced with
// chrome/browser/resources/settings/autofill_page/password_manager_proxy.ts.
enum class PasswordManagerPage {
  kPasswords = 0,
  kCheckup,
  kLastItem = kCheckup
};

}  // namespace

PasswordManagerHandler::PasswordManagerHandler() = default;
PasswordManagerHandler::~PasswordManagerHandler() = default;

void PasswordManagerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "showPasswordManager",
      base::BindRepeating(&PasswordManagerHandler::HandleShowPasswordManager,
                          base::Unretained(this)));
}

void PasswordManagerHandler::HandleShowPasswordManager(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  int page = args[0].GetInt();

  Browser* current_broswer =
      chrome::FindBrowserWithTab(web_ui()->GetWebContents());
  CHECK(current_broswer);

  switch (PasswordManagerPage(page)) {
    case PasswordManagerPage::kPasswords:
      chrome::ShowPasswordManager(current_broswer);
      return;
    case PasswordManagerPage::kCheckup:
      chrome::ShowPasswordCheck(current_broswer);
      return;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace settings
