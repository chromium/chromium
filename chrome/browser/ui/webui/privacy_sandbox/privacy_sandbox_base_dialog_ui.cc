// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_base_dialog_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/privacy_sandbox_resources.h"
#include "chrome/grit/privacy_sandbox_resources_map.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace privacy_sandbox {

PrivacySandboxBaseDialogUI::PrivacySandboxBaseDialogUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui),
      chrome::kChromeUIPrivacySandboxBaseDialogHost);
  webui::SetupWebUIDataSource(source, kPrivacySandboxResources,
                              IDR_PRIVACY_SANDBOX_BASE_DIALOG_HTML);

  static constexpr webui::LocalizedString kStrings[] = {
      {"adPrivacyPageTitle", IDS_SETTINGS_AD_PRIVACY_PAGE_TITLE}};

  source->AddLocalizedStrings(kStrings);
}

WEB_UI_CONTROLLER_TYPE_IMPL(PrivacySandboxBaseDialogUI)

PrivacySandboxBaseDialogUI::~PrivacySandboxBaseDialogUI() = default;

}  // namespace privacy_sandbox
