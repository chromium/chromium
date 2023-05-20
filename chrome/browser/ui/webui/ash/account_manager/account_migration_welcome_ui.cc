// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/account_manager/account_migration_welcome_ui.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/signin/ash/inline_login_dialog.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace ash {

namespace {

class MigrationMessageHandler : public content::WebUIMessageHandler {
 public:
  explicit MigrationMessageHandler(base::RepeatingClosure close_dialog_closure)
      : close_dialog_closure_(close_dialog_closure) {}

  MigrationMessageHandler(const MigrationMessageHandler&) = delete;
  MigrationMessageHandler& operator=(const MigrationMessageHandler&) = delete;

  ~MigrationMessageHandler() override = default;

 private:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "reauthenticateAccount",
        base::BindRepeating(
            &MigrationMessageHandler::HandleReauthenticateAccount,
            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "closeDialog",
        base::BindRepeating(&MigrationMessageHandler::HandleCloseDialog,
                            base::Unretained(this)));
  }

  // WebUI "reauthenticateAccount" message callback.
  void HandleReauthenticateAccount(const base::Value::List& args) {
    AllowJavascript();

    CHECK(!args.empty());
    const std::string& account_email = args[0].GetString();

    Profile* profile = Profile::FromWebUI(web_ui());
    ::GetAccountManagerFacade(profile->GetPath().value())
        ->ShowReauthAccountDialog(
            account_manager::AccountManagerFacade::AccountAdditionSource::
                kAccountManagerMigrationWelcomeScreen,
            account_email, base::DoNothing());
    HandleCloseDialog(args);
  }

  void HandleCloseDialog(const base::Value::List& args) {
    AllowJavascript();

    close_dialog_closure_.Run();
  }

  base::RepeatingClosure close_dialog_closure_;
};

}  // namespace

AccountMigrationWelcomeUI::AccountMigrationWelcomeUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          Profile::FromWebUI(web_ui),
          chrome::kChromeUIAccountMigrationWelcomeHost);
  webui::SetJSModuleDefaults(html_source);
  webui::EnableTrustedTypesCSP(html_source);

  // Add localized strings.
  html_source->AddLocalizedString("welcomePageTitle",
                                  IDS_ACCOUNT_MIGRATION_WELCOME_PAGE_TITLE);
  html_source->AddLocalizedString("welcomeTitle",
                                  IDS_ACCOUNT_MIGRATION_WELCOME_TITLE);
  html_source->AddLocalizedString("welcomeMessage",
                                  IDS_ACCOUNT_MIGRATION_WELCOME_TEXT);
  html_source->AddLocalizedString("cancelButton", IDS_APP_CANCEL);
  html_source->AddLocalizedString("migrateButton",
                                  IDS_ACCOUNT_MIGRATION_UPDATE_BUTTON);
  html_source->AddString("accountManagerLearnMoreUrl",
                         chrome::kAccountManagerLearnMoreURL);

  // Add required resources.
  html_source->AddResourcePath("account_migration_welcome_app.js",
                               IDR_ACCOUNT_MIGRATION_WELCOME_APP_JS);
  html_source->AddResourcePath("account_migration_welcome_app.html.js",
                               IDR_ACCOUNT_MIGRATION_WELCOME_APP_HTML_JS);
  html_source->AddResourcePath("account_manager_shared.css.js",
                               IDR_ACCOUNT_MANAGER_SHARED_CSS_JS);
  html_source->AddResourcePath("account_manager_browser_proxy.js",
                               IDR_ACCOUNT_MANAGER_BROWSER_PROXY_JS);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  html_source->AddResourcePath("account_manager_welcome_1x.png",
                               IDR_ACCOUNT_MANAGER_WELCOME_1X_PNG);
  html_source->AddResourcePath("account_manager_welcome_2x.png",
                               IDR_ACCOUNT_MANAGER_WELCOME_2X_PNG);
  html_source->AddResourcePath("googleg.svg",
                               IDR_ACCOUNT_MANAGER_WELCOME_GOOGLE_LOGO_SVG);
#endif
  html_source->SetDefaultResource(IDR_ACCOUNT_MIGRATION_WELCOME_HTML);

  web_ui->AddMessageHandler(std::make_unique<MigrationMessageHandler>(
      base::BindRepeating(&WebDialogUI::CloseDialog, weak_factory_.GetWeakPtr(),
                          base::Value::List() /* args */)));
}

AccountMigrationWelcomeUI::~AccountMigrationWelcomeUI() = default;

}  // namespace ash
