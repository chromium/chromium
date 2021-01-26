// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/work_profile_confirmation_ui.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/work_profile_confirmation_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"

namespace {
constexpr char kManagedLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=is_chrome_managed";
}

WorkProfileConfirmationUI::WorkProfileConfirmationUI(content::WebUI* web_ui)
    : SigninWebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIWorkProfileConfirmationHost);
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();

  source->AddResourcePath("signin_shared_css.js", IDR_SIGNIN_SHARED_CSS_JS);
  source->AddResourcePath("work_profile_confirmation_browser_proxy.js",
                          IDR_WORK_PROFILE_CONFIRMATION_BROWSER_PROXY_JS);
  source->AddResourcePath("work_profile_confirmation.js",
                          IDR_WORK_PROFILE_CONFIRMATION_JS);

  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER_HTML);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");

  source->SetDefaultResource(IDR_WORK_PROFILE_CONFIRMATION_HTML);
  source->AddResourcePath("work_profile_confirmation_app.js",
                          IDR_WORK_PROFILE_CONFIRMATION_APP_JS);

  source->AddResourcePath(
      "images/work_profile_confirmation_illustration.svg",
      IDR_WORK_PROFILE_CONFIRMATION_IMAGES_WORK_PROFILE_CONFIRMATION_ILLUSTRATION_SVG);
  source->AddResourcePath(
      "images/work_profile_confirmation_illustration_dark.svg",
      IDR_WORK_PROFILE_CONFIRMATION_IMAGES_WORK_PROFILE_CONFIRMATION_ILLUSTRATION_DARK_SVG);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  base::Optional<AccountInfo> primary_account_info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          identity_manager->GetPrimaryAccountInfo());

  if (primary_account_info) {
    source->AddString("workProfileConfirmationInfo",
                      l10n_util::GetStringFUTF16(
                          IDS_WORK_PROFILE_CONFIRMATION_WORK_PROFILE_INFO,
                          base::ASCIIToUTF16(kManagedLearnMoreUrl),
                          base::UTF8ToUTF16(gaia::ExtractDomainName(
                              primary_account_info->email))));
  } else {
    source->AddString(
        "workProfileConfirmationInfo",
        l10n_util::GetStringFUTF16(
            IDS_WORK_PROFILE_CONFIRMATION_WORK_PROFILE_INFO_UNKNOWN_ORGANIZATION,
            base::ASCIIToUTF16(kManagedLearnMoreUrl)));
  }

  source->AddString("accountPictureUrl",
                    profiles::GetPlaceholderAvatarIconUrl());
  source->AddLocalizedString("workProfileConfirmationTitle",
                             IDS_WORK_PROFILE_CONFIRMATION_TITLE);
  source->AddLocalizedString(
      "workProfileConfirmationConfirmLabel",
      IDS_WORK_PROFILE_CONFIRMATION_CONFIRM_BUTTON_LABEL);
  source->AddLocalizedString("workProfileConfirmationCancelLabel", IDS_CANCEL);

  source->DisableTrustedTypesCSP();

  base::DictionaryValue strings;
  webui::SetLoadTimeDataDefaults(g_browser_process->GetApplicationLocale(),
                                 &strings);
  source->AddLocalizedStrings(strings);

  content::WebUIDataSource::Add(profile, source);
}

WorkProfileConfirmationUI::~WorkProfileConfirmationUI() = default;

void WorkProfileConfirmationUI::InitializeMessageHandlerWithBrowser(
    Browser* browser) {
  NOTREACHED();
}

void WorkProfileConfirmationUI::InitializeMessageHandlerWithBrowser(
    Browser* browser,
    DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
  web_ui()->AddMessageHandler(std::make_unique<WorkProfileConfirmationHandler>(
      browser->profile(), browser, std::move(callback)));
}

void WorkProfileConfirmationUI::InitializeMessageHandlerWithProfile(
    Profile* profile,
    DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
  web_ui()->AddMessageHandler(std::make_unique<WorkProfileConfirmationHandler>(
      profile, nullptr, std::move(callback)));
}
