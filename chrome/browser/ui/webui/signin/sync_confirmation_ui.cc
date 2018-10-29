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
#include "components/signin/core/browser/avatar_icon_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/unified_consent/feature.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

SyncConfirmationUI::SyncConfirmationUI(content::WebUI* web_ui)
    : SigninWebDialogUI(web_ui),
      consent_feature_(consent_auditor::Feature::CHROME_SYNC) {
  Profile* profile = Profile::FromWebUI(web_ui);
  bool is_sync_allowed = profile->IsSyncAllowed();
  bool is_unified_consent_enabled =
      unified_consent::IsUnifiedConsentFeatureEnabled();

  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISyncConfirmationHost);
  source->SetJsonPath("strings.js");
  source->AddResourcePath("signin_shared_css.html", IDR_SIGNIN_SHARED_CSS_HTML);

  int title_ids = -1;
  int confirm_button_ids = -1;
  int undo_button_ids = -1;
  if (is_unified_consent_enabled && is_sync_allowed) {
    source->SetDefaultResource(IDR_DICE_SYNC_CONFIRMATION_HTML);
    source->AddResourcePath("icons.html",
                            IDR_DICE_SYNC_CONFIRMATION_ICONS_HTML);
    source->AddResourcePath("sync_confirmation_browser_proxy.html",
                            IDR_DICE_SYNC_CONFIRMATION_BROWSER_PROXY_HTML);
    source->AddResourcePath("sync_confirmation_browser_proxy.js",
                            IDR_DICE_SYNC_CONFIRMATION_BROWSER_PROXY_JS);
    source->AddResourcePath("sync_confirmation_app.html",
                            IDR_DICE_SYNC_CONFIRMATION_APP_HTML);
    source->AddResourcePath("sync_confirmation_app.js",
                            IDR_DICE_SYNC_CONFIRMATION_APP_JS);
    source->AddResourcePath("sync_confirmation.js",
                            IDR_DICE_SYNC_CONFIRMATION_JS);

    AddStringResource(source, "syncConfirmationChromeSyncBody",
                      IDS_SYNC_CONFIRMATION_DICE_CHROME_SYNC_MESSAGE);
    AddStringResource(source, "syncConfirmationPersonalizeServicesBody",
                      IDS_SYNC_CONFIRMATION_DICE_PERSONALIZE_SERVICES_BODY);
    AddStringResource(source, "syncConfirmationGoogleServicesBody",
                      IDS_SYNC_CONFIRMATION_DICE_GOOGLE_SERVICES_BODY);
    AddStringResource(source, "syncConfirmationSyncSettingsLinkBody",
                      IDS_SYNC_CONFIRMATION_DICE_SYNC_SETTINGS_LINK_BODY);
    AddStringResource(source, "syncConfirmationSyncSettingsDescription",
                      IDS_SYNC_CONFIRMATION_DICE_SYNC_SETTINGS_DESCRIPTION);
    AddStringResource(source, "syncConfirmationSettingsLabel",
                      IDS_SYNC_CONFIRMATION_DICE_SETTINGS_BUTTON_LABEL);

    AddStringResource(source, "syncConfirmationMoreOptionsLabel",
                      IDS_SYNC_CONFIRMATION_UNITY_MORE_OPTIONS_BUTTON_LABEL);
    AddStringResource(source, "syncConfirmationOptionsTitle",
                      IDS_SYNC_CONFIRMATION_UNITY_MORE_OPTIONS_TITLE);
    AddStringResource(source, "syncConfirmationOptionsSubtitle",
                      IDS_SYNC_CONFIRMATION_UNITY_MORE_OPTIONS_SUBTITLE);
    AddStringResource(
        source, "syncConfirmationOptionsReviewSettingsTitle",
        IDS_SYNC_CONFIRMATION_UNITY_OPTIONS_REVIEW_SETTINGS_TITLE);
    AddStringResource(
        source, "syncConfirmationOptionsMakeNoChangesTitle",
        IDS_SYNC_CONFIRMATION_UNITY_OPTIONS_MAKE_NO_CHANGES_TITLE);
    AddStringResource(
        source, "syncConfirmationOptionsMakeNoChangesSubtitle",
        IDS_SYNC_CONFIRMATION_UNITY_OPTIONS_MAKE_NO_CHANGES_SUBTITLE);
    AddStringResource(source, "syncConfirmationOptionsUseDefaultTitle",
                      IDS_SYNC_CONFIRMATION_UNITY_OPTIONS_USE_DEFAULT_TITLE);
    AddStringResource(source, "syncConfirmationOptionsUseDefaultSubtitle",
                      IDS_SYNC_CONFIRMATION_UNITY_OPTIONS_USE_DEFAULT_SUBTITLE);
    AddStringResource(source, "syncConfirmationOptionsConfirmLabel", IDS_OK);
    AddStringResource(source, "syncConfirmationOptionsBackLabel",
                      IDS_SYNC_CONFIRMATION_UNITY_OPTIONS_BACK_BUTTON_LABEL);
    AddStringResource(source, "syncConsentBumpTitle",
                      IDS_SYNC_CONFIRMATION_UNITY_CONSENT_BUMP_TITLE);

    constexpr int kAccountPictureSize = 68;
    std::string custom_picture_url = profiles::GetPlaceholderAvatarIconUrl();
    GURL account_picture_url(IdentityManagerFactory::GetForProfile(profile)
                                 ->GetPrimaryAccountInfo()
                                 .picture_url);
    if (account_picture_url.is_valid()) {
      custom_picture_url = signin::GetAvatarImageURLWithOptions(
                               account_picture_url, kAccountPictureSize,
                               false /* no_silhouette */)
                               .spec();
    }
    source->AddString("accountPictureUrl", custom_picture_url);

    title_ids = IDS_SYNC_CONFIRMATION_UNITY_TITLE;
    confirm_button_ids = IDS_SYNC_CONFIRMATION_DICE_CONFIRM_BUTTON_LABEL;
    undo_button_ids = IDS_CANCEL;
    consent_feature_ = consent_auditor::Feature::CHROME_UNIFIED_CONSENT;
  } else {
    source->SetDefaultResource(IDR_SYNC_CONFIRMATION_HTML);
    source->AddResourcePath("sync_confirmation.css", IDR_SYNC_CONFIRMATION_CSS);
    source->AddResourcePath("sync_confirmation.js", IDR_SYNC_CONFIRMATION_JS);

    source->AddBoolean("isSyncAllowed", is_sync_allowed);

    AddStringResource(source, "syncConfirmationChromeSyncTitle",
                      IDS_SYNC_CONFIRMATION_CHROME_SYNC_TITLE);
    AddStringResource(source, "syncConfirmationChromeSyncBody",
                      IDS_SYNC_CONFIRMATION_CHROME_SYNC_MESSAGE);
    AddStringResource(source, "syncConfirmationPersonalizeServicesTitle",
                      IDS_SYNC_CONFIRMATION_PERSONALIZE_SERVICES_TITLE);
    AddStringResource(source, "syncConfirmationPersonalizeServicesBody",
                      IDS_SYNC_CONFIRMATION_PERSONALIZE_SERVICES_BODY);
    AddStringResource(source, "syncConfirmationSyncSettingsLinkBody",
                      IDS_SYNC_CONFIRMATION_SYNC_SETTINGS_LINK_BODY);
    AddStringResource(source, "syncDisabledConfirmationDetails",
                      IDS_SYNC_DISABLED_CONFIRMATION_DETAILS);

    title_ids = AccountConsistencyModeManager::IsDiceEnabledForProfile(profile)
                    ? IDS_SYNC_CONFIRMATION_DICE_TITLE
                    : IDS_SYNC_CONFIRMATION_TITLE;
    confirm_button_ids = IDS_SETTINGS_TURN_ON;
    undo_button_ids = IDS_CANCEL;
    consent_feature_ = consent_auditor::Feature::CHROME_SYNC;
    if (!is_sync_allowed) {
      title_ids = IDS_SYNC_DISABLED_CONFIRMATION_CHROME_SYNC_TITLE;
      confirm_button_ids = IDS_SYNC_DISABLED_CONFIRMATION_CONFIRM_BUTTON_LABEL;
      undo_button_ids = IDS_SYNC_DISABLED_CONFIRMATION_UNDO_BUTTON_LABEL;
    }
  }

  DCHECK_GE(title_ids, 0);
  DCHECK_GE(confirm_button_ids, 0);
  DCHECK_GE(undo_button_ids, 0);

  AddStringResource(source, "syncConfirmationTitle", title_ids);
  AddStringResource(source, "syncConfirmationConfirmLabel", confirm_button_ids);
  AddStringResource(source, "syncConfirmationUndoLabel", undo_button_ids);

  base::DictionaryValue strings;
  webui::SetLoadTimeDataDefaults(
      g_browser_process->GetApplicationLocale(), &strings);
  source->AddLocalizedStrings(strings);

  content::WebUIDataSource::Add(profile, source);
}

SyncConfirmationUI::~SyncConfirmationUI() {}

void SyncConfirmationUI::InitializeMessageHandlerWithBrowser(Browser* browser) {
  web_ui()->AddMessageHandler(std::make_unique<SyncConfirmationHandler>(
      browser, js_localized_string_to_ids_map_, consent_feature_));
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
