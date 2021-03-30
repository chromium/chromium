// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_reauth_ui.h"

#include <string>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/reauth_util.h"
#include "chrome/browser/ui/signin_reauth_view_controller.h"
#include "chrome/browser/ui/webui/signin/signin_reauth_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "google_apis/gaia/core_account_id.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"

namespace {

std::string GetAccountImageURL(Profile* profile) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  // The current version of the reauth only supports the primary account.
  // TODO(crbug.com/1083429): generalize for arbitrary accounts by passing an
  // account id as a method parameter.
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  // Sync shouldn't be enabled. Otherwise, the primary account and the first
  // cookie account may diverge.
  DCHECK(!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  base::Optional<AccountInfo> account_info =
      identity_manager
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              account_id);

  return account_info && !account_info->account_image.IsEmpty()
             ? webui::GetBitmapDataUrl(account_info->account_image.AsBitmap())
             : profiles::GetPlaceholderAvatarIconUrl();
}

}  // namespace

SigninReauthUI::SigninReauthUI(content::WebUI* web_ui)
    : SigninWebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISigninReauthHost);
  webui::SetJSModuleDefaults(source);
  source->SetDefaultResource(IDR_SIGNIN_REAUTH_HTML);

  static constexpr webui::ResourcePath kResources[] = {
      {"signin_reauth_app.js", IDR_SIGNIN_REAUTH_APP_JS},
      {"signin_reauth_browser_proxy.js", IDR_SIGNIN_REAUTH_BROWSER_PROXY_JS},
      {"signin_shared_css.js", IDR_SIGNIN_SHARED_CSS_JS},
      {"signin_vars_css.js", IDR_SIGNIN_VARS_CSS_JS},
      // Resources for the account passwords reauth.
      {"images/signin_reauth_illustration.svg",
       IDR_SIGNIN_REAUTH_IMAGES_ACCOUNT_PASSWORDS_REAUTH_ILLUSTRATION_SVG},
      {"images/signin_reauth_illustration_dark.svg",
       IDR_SIGNIN_REAUTH_IMAGES_ACCOUNT_PASSWORDS_REAUTH_ILLUSTRATION_DARK_SVG},
  };
  source->AddResourcePaths(kResources);

  source->AddString("accountImageUrl", GetAccountImageURL(profile));

  AddStringResource(source, "signinReauthTitle",
                    IDS_ACCOUNT_PASSWORDS_REAUTH_TITLE);
  AddStringResource(source, "signinReauthDesc",
                    IDS_ACCOUNT_PASSWORDS_REAUTH_DESC);
  AddStringResource(source, "signinReauthConfirmLabel",
                    IDS_ACCOUNT_PASSWORDS_REAUTH_CONFIRM_BUTTON_LABEL);
  AddStringResource(source, "signinReauthCloseLabel",
                    IDS_ACCOUNT_PASSWORDS_REAUTH_CLOSE_BUTTON_LABEL);

  content::WebUIDataSource::Add(profile, source);
}

SigninReauthUI::~SigninReauthUI() = default;

void SigninReauthUI::InitializeMessageHandlerWithReauthController(
    SigninReauthViewController* controller) {
  web_ui()->AddMessageHandler(std::make_unique<SigninReauthHandler>(
      controller,
      base::flat_map<std::string, int>(js_localized_string_to_ids_)));
}

void SigninReauthUI::InitializeMessageHandlerWithBrowser(Browser* browser) {}

void SigninReauthUI::AddStringResource(content::WebUIDataSource* source,
                                       base::StringPiece name,
                                       int ids) {
  source->AddLocalizedString(name, ids);

  // When the strings are passed to the HTML, the Unicode NBSP symbol (\u00A0)
  // will be automatically replaced with "&nbsp;". This change must be mirrored
  // in the string-to-ids map. Note that "\u00A0" is actually two characters,
  // so we must use base::ReplaceSubstrings* rather than base::ReplaceChars.
  // TODO(treib): De-dupe this with the similar code in SyncConfirmationUI,
  // SyncConsentScreenHandler, and possibly other places.
  std::string sanitized_string =
      base::UTF16ToUTF8(l10n_util::GetStringUTF16(ids));
  base::ReplaceSubstringsAfterOffset(&sanitized_string, 0, "\u00A0" /* NBSP */,
                                     "&nbsp;");

  js_localized_string_to_ids_.emplace_back(sanitized_string, ids);
}

WEB_UI_CONTROLLER_TYPE_IMPL(SigninReauthUI)
