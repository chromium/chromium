// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/support_tool/support_tool_ui.h"
#include <string>

#include <string>

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/support_tool_resources.h"
#include "chrome/grit/support_tool_resources_map.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

const char kSupportCaseIDQuery[] = "case_id";

namespace {
// Returns the support case ID that's extracted from `url` with query
// `kSupportCaseIDQuery`. Returns empty string if `url` doesn't contain support
// case ID.
std::string GetSupportCaseIDFromURL(const GURL& url) {
  std::string support_case_id;
  if (url.has_query()) {
    net::GetValueForKeyInQuery(url, kSupportCaseIDQuery, &support_case_id);
  }
  return support_case_id;
}

content::WebUIDataSource* CreateSupportToolHTMLSource(const GURL& url) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISupportToolHost);

  source->AddString("caseId", GetSupportCaseIDFromURL(url));

  webui::SetupWebUIDataSource(
      source, base::make_span(kSupportToolResources, kSupportToolResourcesSize),
      IDR_SUPPORT_TOOL_SUPPORT_TOOL_CONTAINER_HTML);

  return source;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// SupportToolMessageHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages related to Support Tool.
class SupportToolMessageHandler : public content::WebUIMessageHandler {
 public:
  SupportToolMessageHandler() = default;

  SupportToolMessageHandler(const SupportToolMessageHandler&) = delete;
  SupportToolMessageHandler& operator=(const SupportToolMessageHandler&) =
      delete;

  ~SupportToolMessageHandler() override = default;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  void HandleGetEmailAddresses(const base::Value::List& args);

 private:
  base::Value::List GetAccountsList();

  base::WeakPtrFactory<SupportToolMessageHandler> weak_ptr_factory_{this};
};

void SupportToolMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getEmailAddresses",
      base::BindRepeating(&SupportToolMessageHandler::HandleGetEmailAddresses,
                          weak_ptr_factory_.GetWeakPtr()));
}

base::Value::List SupportToolMessageHandler::GetAccountsList() {
  Profile* profile = Profile::FromWebUI(web_ui());
  base::Value::List account_list;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile)) {
    // If dice is enabled, show all the accounts.
    for (const auto& account :
         signin_ui_util::GetAccountsForDicePromos(profile)) {
      if (!account.IsEmpty())
        account_list.Append(base::Value(account.email));
    }
    return account_list;
  }
#endif

  // Guest mode does not have a primary account (or an IdentityManager).
  if (profile->IsGuestSession())
    return account_list;

  // If DICE is disabled for this profile or unsupported on this platform (e.g.
  // Chrome OS), then show only the primary account, whether or not that account
  // has consented to sync.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  AccountInfo primary_account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  if (!primary_account_info.IsEmpty())
    account_list.Append(base::Value(primary_account_info.email));
  return account_list;
}

void SupportToolMessageHandler::HandleGetEmailAddresses(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, base::Value(GetAccountsList()));
}

////////////////////////////////////////////////////////////////////////////////
//
// SupportToolUI
//
////////////////////////////////////////////////////////////////////////////////

SupportToolUI::SupportToolUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<SupportToolMessageHandler>());

  // Set up the chrome://support-tool/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(
      profile, CreateSupportToolHTMLSource(web_ui->GetWebContents()->GetURL()));
}

SupportToolUI::~SupportToolUI() = default;
