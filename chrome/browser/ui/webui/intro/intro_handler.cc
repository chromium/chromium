// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/intro_handler.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/browser/ui/webui/intro/policy_store_observer.h"
#include "chrome/grit/branded_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

IntroHandler::IntroHandler(
    base::RepeatingCallback<void(IntroChoice)> intro_callback,
    base::OnceCallback<void(DefaultBrowserChoice)> default_browser_callback,
    bool is_device_managed,
    std::string_view source_name)
    : intro_callback_(std::move(intro_callback)),
      default_browser_callback_(std::move(default_browser_callback)),
      is_device_managed_(is_device_managed),
      source_name_(source_name) {
  DCHECK(intro_callback_);
  DCHECK(default_browser_callback_);
}

IntroHandler::~IntroHandler() = default;

void IntroHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "continueWithoutAccount",
      base::BindRepeating(&IntroHandler::HandleContinueWithoutAccount,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "continueWithAccount",
      base::BindRepeating(&IntroHandler::HandleContinueWithAccount,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initializeMainView",
      base::BindRepeating(&IntroHandler::HandleInitializeMainView,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setAsDefaultBrowser",
      base::BindRepeating(&IntroHandler::HandleSetAsDefaultBrowser,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "skipDefaultBrowser",
      base::BindRepeating(&IntroHandler::HandleSkipDefaultBrowser,
                          base::Unretained(this)));
}

void IntroHandler::OnJavascriptAllowed() {
  if (!is_device_managed_) {
    return;
  }
  policy_store_observer_ = std::make_unique<PolicyStoreObserver>(base::BindOnce(
      &IntroHandler::FireManagedDisclaimerUpdate, base::Unretained(this)));
}

void IntroHandler::HandleContinueWithAccount(const base::ListValue& args) {
  CHECK(args.empty());
  intro_callback_.Run(IntroChoice::kContinueWithAccount);
}

void IntroHandler::HandleContinueWithoutAccount(const base::ListValue& args) {
  CHECK(args.empty());
  intro_callback_.Run(IntroChoice::kContinueWithoutAccount);
}

void IntroHandler::ResetIntroButtons() {
  if (IsJavascriptAllowed()) {
    FireWebUIListener("reset-intro-buttons");
  }
}

void IntroHandler::HandleInitializeMainView(const base::ListValue& args) {
  CHECK(args.empty());
  AllowJavascript();
}

void IntroHandler::HandleSetAsDefaultBrowser(const base::ListValue& args) {
  CHECK(args.empty());
  if (default_browser_callback_) {
    std::move(default_browser_callback_)
        .Run(DefaultBrowserChoice::kClickSetAsDefault);
  }
}

void IntroHandler::HandleSkipDefaultBrowser(const base::ListValue& args) {
  CHECK(args.empty());
  if (default_browser_callback_) {
    std::move(default_browser_callback_).Run(DefaultBrowserChoice::kSkip);
  }
}

void IntroHandler::ResetDefaultBrowserButtons() {
  if (IsJavascriptAllowed()) {
    FireWebUIListener("reset-default-browser-buttons");
  }
}

void IntroHandler::SetCanPinToTaskbar(bool can_pin) {
  if (can_pin) {
    base::DictValue update;
    update.Set(
        "defaultBrowserTitle",
        l10n_util::GetStringUTF16(IDS_FRE_DEFAULT_BROWSER_AND_PINNING_TITLE));
    update.Set("defaultBrowserSubtitle",
               l10n_util::GetStringUTF16(
                   IDS_FRE_DEFAULT_BROWSER_AND_PINNING_SUBTITLE));
    content::WebUIDataSource::Update(Profile::FromWebUI(web_ui()), source_name_,
                                     std::move(update));
  }
}

void IntroHandler::FireManagedDisclaimerUpdate(std::string disclaimer) {
  DCHECK(is_device_managed_);
  if (IsJavascriptAllowed()) {
    FireWebUIListener("managed-device-disclaimer-updated",
                      base::Value(std::move(disclaimer)));
  }
}
