// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/user_manager_ui.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/webui/signin/signin_create_profile_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/user_manager_screen_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

UserManagerUI::UserManagerUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  auto signin_create_profile_handler =
      std::make_unique<SigninCreateProfileHandler>();
  signin_create_profile_handler_ = signin_create_profile_handler.get();
  web_ui->AddMessageHandler(std::move(signin_create_profile_handler));
  auto user_manager_screen_handler =
      std::make_unique<UserManagerScreenHandler>();
  user_manager_screen_handler_ = user_manager_screen_handler.get();
  web_ui->AddMessageHandler(std::move(user_manager_screen_handler));

  base::DictionaryValue localized_strings;
  GetLocalizedStrings(&localized_strings);

  Profile* profile = Profile::FromWebUI(web_ui);

  // Set up the chrome://md-user-manager/ source.
  content::WebUIDataSource::Add(profile, CreateUIDataSource(localized_strings));

  // Set up the chrome://theme/ source
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
}

UserManagerUI::~UserManagerUI() {}

content::WebUIDataSource* UserManagerUI::CreateUIDataSource(
    const base::DictionaryValue& localized_strings) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIMdUserManagerHost);
  source->AddLocalizedStrings(localized_strings);
  source->AddBoolean("profileShortcutsEnabled",
                     ProfileShortcutManager::IsFeatureEnabled());
  source->AddBoolean("isForceSigninEnabled",
                     signin_util::IsForceSigninEnabled());

  source->UseStringsJs();

  source->AddResourcePath("control_bar.html", IDR_CONTROL_BAR_HTML);
  source->AddResourcePath("control_bar.js", IDR_CONTROL_BAR_JS);
  source->AddResourcePath("create_profile.html", IDR_CREATE_PROFILE_HTML);
  source->AddResourcePath("create_profile.js", IDR_CREATE_PROFILE_JS);
  source->AddResourcePath("error_dialog.html", IDR_ERROR_DIALOG_HTML);
  source->AddResourcePath("error_dialog.js", IDR_ERROR_DIALOG_JS);
  source->AddResourcePath("profile_browser_proxy.html",
                          IDR_PROFILE_BROWSER_PROXY_HTML);
  source->AddResourcePath("profile_browser_proxy.js",
                          IDR_PROFILE_BROWSER_PROXY_JS);
  source->AddResourcePath("shared_styles.html",
                          IDR_USER_MANAGER_SHARED_STYLES_HTML);
  source->AddResourcePath("strings.html", IDR_USER_MANAGER_STRINGS_HTML);
  source->AddResourcePath("user_manager.js", IDR_USER_MANAGER_JS);
  source->AddResourcePath("user_manager_pages.html",
                          IDR_USER_MANAGER_PAGES_HTML);
  source->AddResourcePath("user_manager_pages.js", IDR_USER_MANAGER_PAGES_JS);
  source->AddResourcePath("user_manager_tutorial.html",
                          IDR_USER_MANAGER_TUTORIAL_HTML);
  source->AddResourcePath("user_manager_tutorial.js",
                          IDR_USER_MANAGER_TUTORIAL_JS);

  source->SetDefaultResource(IDR_USER_MANAGER_HTML);

  return source;
}

void UserManagerUI::GetLocalizedStrings(
    base::DictionaryValue* localized_strings) {
  user_manager_screen_handler_->GetLocalizedValues(localized_strings);
  signin_create_profile_handler_->GetLocalizedValues(localized_strings);
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, localized_strings);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  localized_strings->SetString("buildType", "chrome");
#else
  localized_strings->SetString("buildType", "chromium");
#endif
}
