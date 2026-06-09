// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_ui.h"

#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/privacy_sandbox_internals_resources.h"
#include "chrome/grit/privacy_sandbox_internals_resources_map.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "privacy_sandbox_internals_ui.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/webui_util.h"

namespace privacy_sandbox_internals {

PrivacySandboxInternalsUI::PrivacySandboxInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIPrivacySandboxInternalsHost);
  webui::SetupWebUIDataSource(source, kPrivacySandboxInternalsResources,
                              IDR_PRIVACY_SANDBOX_INTERNALS_INDEX_HTML);

  // Adds a flag boolean to UI source, mirroring kPrivacySandboxInternalsDevUI
  // flag.
  source->AddBoolean("isPrivacySandboxInternalsDevUIEnabled",
                     base::FeatureList::IsEnabled(
                         privacy_sandbox::kPrivacySandboxInternalsDevUI));
}

PrivacySandboxInternalsUI::~PrivacySandboxInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(PrivacySandboxInternalsUI)

}  // namespace privacy_sandbox_internals
