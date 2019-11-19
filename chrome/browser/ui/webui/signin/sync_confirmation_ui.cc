// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"

SyncConfirmationUI::SyncConfirmationUI(content::WebUI* web_ui)
    : SigninWebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  bool is_sync_allowed = profile->IsSyncAllowed();

  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISyncConfirmationHost);
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();

  if (is_sync_allowed) {
    source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER);
    source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER);
    source->OverrideContentSecurityPolicyScriptSrc(
        "script-src chrome://resources chrome://test 'self';");

    source->SetDefaultResource(IDR_SYNC_CONFIRMATION_HTML);
    source->AddResourcePath("signin_shared_css.js", IDR_SIGNIN_SHARED_CSS_JS);
    source->AddResourcePath("sync_confirmation_browser_proxy.js",
                            IDR_SYNC_CONFIRMATION_BROWSER_PROXY_JS);
    source->AddResourcePath("sync_confirmation_app.js",
                            IDR_SYNC_CONFIRMATION_APP_JS);
    source->AddResourcePath("sync_confirmation.js", IDR_SYNC_CONFIRMATION_JS);

    source->AddResourcePath(
        "images/sync_confirmation_illustration.svg",
        IDR_SYNC_CONFIRMATION_IMAGES_SYNC_CONFIRMATION_ILLUSTRATION_SVG);
    source->AddResourcePath(
        "images/sync_confirmation_illustration_dark.svg",
        IDR_SYNC_CONFIRMATION_IMAGES_SYNC_CONFIRMATION_ILLUSTRATION_DARK_SVG);

    AddStringResource(source, "syncConfirmationTitle",
                      IDS_SYNC_CONFIRMATION_TITLE);
    AddStringResource(source, "syncConfirmationSyncInfoTitle",
                      IDS_SYNC_CONFIRMATION_SYNC_INFO_TITLE);
    AddStringResource(source, "syncConfirmationSyncInfoDesc",
                      IDS_SYNC_CONFIRMATION_SYNC_INFO_DESC);
    AddStringResource(source, "syncConfirmationSettingsInfo",
                      IDS_SYNC_CONFIRMATION_SETTINGS_INFO);
    AddStringResource(source, "syncConfirmationSettingsLabel",
                      IDS_SYNC_CONFIRMATION_SETTINGS_BUTTON_LABEL);
    AddStringResource(source, "syncConfirmationConfirmLabel",
                      IDS_SYNC_CONFIRMATION_CONFIRM_BUTTON_LABEL);
    AddStringResource(source, "syncConfirmationUndoLabel", IDS_CANCEL);

    constexpr int kAccountPictureSize = 68;
    std::string custom_picture_url = profiles::GetPlaceholderAvatarIconUrl();
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    base::Optional<AccountInfo> primary_account_info =
        identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
            identity_manager->GetPrimaryAccountInfo());
    GURL account_picture_url(primary_account_info
                                 ? primary_account_info->picture_url
                                 : std::string());
    if (account_picture_url.is_valid()) {
      custom_picture_url = signin::GetAvatarImageURLWithOptions(
                               account_picture_url, kAccountPictureSize,
                               false /* no_silhouette */)
                               .spec();
    }
    source->AddString("accountPictureUrl", custom_picture_url);
  } else {
    source->SetDefaultResource(IDR_SYNC_DISABLED_CONFIRMATION_HTML);
    source->AddResourcePath("signin_shared_old_css.html",
                            IDR_SIGNIN_SHARED_OLD_CSS_HTML);
    source->AddResourcePath("sync_disabled_confirmation.js",
                            IDR_SYNC_DISABLED_CONFIRMATION_JS);

    AddStringResource(source, "syncDisabledConfirmationTitle",
                      IDS_SYNC_DISABLED_CONFIRMATION_CHROME_SYNC_TITLE);
    AddStringResource(source, "syncDisabledConfirmationDetails",
                      IDS_SYNC_DISABLED_CONFIRMATION_DETAILS);
    AddStringResource(source, "syncDisabledConfirmationConfirmLabel",
                      IDS_SYNC_DISABLED_CONFIRMATION_CONFIRM_BUTTON_LABEL);
    AddStringResource(source, "syncDisabledConfirmationUndoLabel",
                      IDS_SYNC_DISABLED_CONFIRMATION_UNDO_BUTTON_LABEL);
  }

  base::DictionaryValue strings;
  webui::SetLoadTimeDataDefaults(
      g_browser_process->GetApplicationLocale(), &strings);
  source->AddLocalizedStrings(strings);

  content::WebUIDataSource::Add(profile, source);
}

SyncConfirmationUI::~SyncConfirmationUI() {}

void SyncConfirmationUI::InitializeMessageHandlerWithBrowser(Browser* browser) {
  web_ui()->AddMessageHandler(std::make_unique<SyncConfirmationHandler>(
      browser, js_localized_string_to_ids_map_));
}

void SyncConfirmationUI::AddStringResource(content::WebUIDataSource* source,
                                           const std::string& name,
                                           int ids) {
  source->AddLocalizedString(name, ids);

  // When the strings are passed to the HTML, the Unicode NBSP symbol (\u00A0)
  // will be automatically replaced with "&nbsp;". This change must be mirrored
  // in the string-to-ids map. Note that "\u00A0" is actually two characters,
  // so we must use base::ReplaceSubstrings* rather than base::ReplaceChars.
  // TODO(msramek): Find a more elegant solution.
  std::string sanitized_string =
      base::UTF16ToUTF8(l10n_util::GetStringUTF16(ids));
  base::ReplaceSubstringsAfterOffset(&sanitized_string, 0, "\u00A0" /* NBSP */,
                                     "&nbsp;");

  js_localized_string_to_ids_map_[sanitized_string] = ids;
}
