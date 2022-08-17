// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/password_manager_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/password_manager_resources.h"
#include "chrome/grit/password_manager_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"
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
      {"title", IDS_PASSWORD_MANAGER_UI_TITLE},
  };
  source->AddLocalizedStrings(kStrings);

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
