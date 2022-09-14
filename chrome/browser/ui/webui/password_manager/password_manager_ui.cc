// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/password_manager_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/password_manager_resources.h"
#include "chrome/grit/password_manager_resources_map.h"
#include "components/grit/components_scaled_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

content::WebUIDataSource* CreatePasswordsUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPasswordManagerHost);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kPasswordManagerResources, kPasswordManagerResourcesSize),
      IDR_PASSWORD_MANAGER_PASSWORD_MANAGER_HTML);

  static constexpr webui::LocalizedString kStrings[] = {
      {"addPassword", IDS_PASSWORD_MANAGER_UI_ADD_PASSWORD_BUTTON},
      {"autosigninLabel", IDS_PASSWORD_MANAGER_UI_AUTOSIGNIN_TOGGLE_LABEL},
      {"autosigninDescription", IDS_PASSWORD_MANAGER_UI_AUTOSIGNIN_TOGGLE_DESC},
      {"checkup", IDS_PASSWORD_MANAGER_UI_CHECKUP},
      {"clearSearch", IDS_CLEAR_SEARCH},
      {"exportPasswords", IDS_PASSWORD_MANAGER_UI_EXPORT_BANNER_TITLE},
      {"exportPasswordsDescription", IDS_PASSWORD_MANAGER_UI_EXPORT_BANNER_DESCRIPTION},
      {"importPasswords", IDS_PASSWORD_MANAGER_UI_IMPORT_BANNER_TITLE},
      {"importPasswordsDescription", IDS_PASSWORD_MANAGER_UI_IMPORT_BANNER_DESCRIPTION},
      {"passwords", IDS_PASSWORD_MANAGER_UI_PASSWORDS},
      {"savePasswordsLabel", IDS_PASSWORD_MANAGER_UI_SAVE_PASSWORDS_TOGGLE_LABEL},
      {"searchPrompt", IDS_PASSWORD_MANAGER_UI_SEARCH_PROMPT},
      {"settings", IDS_PASSWORD_MANAGER_UI_SETTINGS},
      {"title", IDS_PASSWORD_MANAGER_UI_TITLE},
      {"trustedVaultBannerLabelOfferOptIn", IDS_PASSWORD_MANAGER_UI_TRUSTED_VAULT_OPT_IN_TITLE},
      {"trustedVaultBannerSubLabelOfferOptIn", IDS_PASSWORD_MANAGER_UI_RUSTED_VAULT_OPT_IN_DESCRIPTION},
  };
  source->AddLocalizedStrings(kStrings);

  source->AddString(
      "passwordsSectionDescription",
      l10n_util::GetStringFUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORDS_DESCRIPTION,
          base::ASCIIToUTF16(chrome::kPasswordManagerLearnMoreURL)));

  return source;
}

}  // namespace

PasswordManagerUI::PasswordManagerUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://password-manager/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  auto* source = CreatePasswordsUIHTMLSource(profile);
  ManagedUIHandler::Initialize(web_ui, source);
  content::WebUIDataSource::Add(profile, source);
}

// static
base::RefCountedMemory* PasswordManagerUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return static_cast<base::RefCountedMemory*>(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
          IDR_PASSWORD_MANAGER_FAVICON, scale_factor));
}
