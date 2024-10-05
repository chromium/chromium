// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_reauth_ui.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/signin/signin_reauth_view_controller.h"
#include "chrome/browser/ui/webui/signin/signin_reauth_handler.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/signin_resources.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "google_apis/gaia/core_account_id.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/webui_resources.h"

namespace {

std::string GetAccountImageURL(Profile* profile) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  // The current version of the reauth only supports the primary account.
  // TODO(crbug.com/40131388): generalize for arbitrary accounts by passing an
  // account id as a method parameter.
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  // Sync shouldn't be enabled. Otherwise, the primary account and the first
  // cookie account may diverge.
  DCHECK(!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(account_id);

  return !account_info.account_image.IsEmpty()
             ? webui::GetBitmapDataUrl(account_info.account_image.AsBitmap())
             : profiles::GetPlaceholderAvatarIconUrl();
}

bool WasPasswordSavedLocally(signin_metrics::ReauthAccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::ReauthAccessPoint::kUnknown:
    case signin_metrics::ReauthAccessPoint::kAutofillDropdown:
    case signin_metrics::ReauthAccessPoint::kPasswordSaveBubble:
    case signin_metrics::ReauthAccessPoint::kPasswordSettings:
    case signin_metrics::ReauthAccessPoint::kGeneratePasswordDropdown:
    case signin_metrics::ReauthAccessPoint::kGeneratePasswordContextMenu:
      return false;
    case signin_metrics::ReauthAccessPoint::kPasswordSaveLocallyBubble:
      return true;
  }
}

int GetReauthDescriptionStringId(
    signin_metrics::ReauthAccessPoint access_point) {
  bool sync_passkeys =
      base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials);
  if (WasPasswordSavedLocally(access_point)) {
    return sync_passkeys
               ? IDS_ACCOUNT_PASSWORDS_WITH_PASSKEYS_REAUTH_DESC_ALREADY_SAVED_LOCALLY
               : IDS_ACCOUNT_PASSWORDS_REAUTH_DESC_ALREADY_SAVED_LOCALLY;
  }
  return sync_passkeys ? IDS_ACCOUNT_PASSWORDS_WITH_PASSKEYS_REAUTH_DESC
                       : IDS_ACCOUNT_PASSWORDS_REAUTH_DESC;
}

int GetReauthCloseButtonLabelStringId(
    signin_metrics::ReauthAccessPoint access_point) {
  if (WasPasswordSavedLocally(access_point)) {
    return IDS_ACCOUNT_PASSWORDS_REAUTH_CLOSE_BUTTON_LABEL_ALREADY_SAVED_LOCALLY;
  }
  return IDS_ACCOUNT_PASSWORDS_REAUTH_CLOSE_BUTTON_LABEL;
}

}  // namespace

bool SigninReauthUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return !profile->IsOffTheRecord();
}

SigninReauthUI::SigninReauthUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUISigninReauthHost);

  static constexpr webui::ResourcePath kResources[] = {
      {"signin_reauth_app.js", IDR_SIGNIN_SIGNIN_REAUTH_SIGNIN_REAUTH_APP_JS},
      {"signin_reauth_app.html.js",
       IDR_SIGNIN_SIGNIN_REAUTH_SIGNIN_REAUTH_APP_HTML_JS},
      {"signin_reauth_app.css.js",
       IDR_SIGNIN_SIGNIN_REAUTH_SIGNIN_REAUTH_APP_CSS_JS},
      {"signin_reauth_browser_proxy.js",
       IDR_SIGNIN_SIGNIN_REAUTH_SIGNIN_REAUTH_BROWSER_PROXY_JS},
      {"signin_shared.css.js", IDR_SIGNIN_SIGNIN_SHARED_CSS_JS},
      {"signin_vars.css.js", IDR_SIGNIN_SIGNIN_VARS_CSS_JS},
      // Resources for the account passwords reauth.
      {"images/signin_reauth_illustration.svg",
       IDR_SIGNIN_SIGNIN_REAUTH_IMAGES_ACCOUNT_PASSWORDS_REAUTH_ILLUSTRATION_SVG},
      {"images/signin_reauth_illustration_dark.svg",
       IDR_SIGNIN_SIGNIN_REAUTH_IMAGES_ACCOUNT_PASSWORDS_REAUTH_ILLUSTRATION_DARK_SVG},
  };

  webui::SetupWebUIDataSource(source, base::make_span(kResources),
                              IDR_SIGNIN_SIGNIN_REAUTH_SIGNIN_REAUTH_HTML);

  source->AddString("accountImageUrl", GetAccountImageURL(profile));

  signin_metrics::ReauthAccessPoint access_point =
      GetReauthAccessPointForReauthConfirmationURL(
          web_ui->GetWebContents()->GetVisibleURL());

  AddStringResource(
      source, "signinReauthTitle",
      base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)
          ? IDS_ACCOUNT_PASSWORDS_WITH_PASSKEYS_REAUTH_TITLE
          : IDS_ACCOUNT_PASSWORDS_REAUTH_TITLE);
  AddStringResource(source, "signinReauthDesc",
                    GetReauthDescriptionStringId(access_point));
  AddStringResource(source, "signinReauthConfirmLabel",
                    IDS_ACCOUNT_PASSWORDS_REAUTH_CONFIRM_BUTTON_LABEL);
  AddStringResource(source, "signinReauthCloseLabel",
                    GetReauthCloseButtonLabelStringId(access_point));
}

SigninReauthUI::~SigninReauthUI() = default;

void SigninReauthUI::InitializeMessageHandlerWithReauthController(
    SigninReauthViewController* controller) {
  web_ui()->AddMessageHandler(std::make_unique<SigninReauthHandler>(
      controller,
      base::flat_map<std::string, int>(js_localized_string_to_ids_)));
}

void SigninReauthUI::AddStringResource(content::WebUIDataSource* source,
                                       std::string_view name,
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
