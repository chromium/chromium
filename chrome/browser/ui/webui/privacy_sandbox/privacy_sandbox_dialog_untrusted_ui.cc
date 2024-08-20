// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_untrusted_ui.h"

#include "chrome/browser/browser_process.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

PrivacySandboxDialogUntrustedUIConfig::PrivacySandboxDialogUntrustedUIConfig()
    : DefaultWebUIConfig(content::kChromeUIUntrustedScheme,
                         chrome::kChromeUIPrivacySandboxDialogHost) {}

PrivacySandboxDialogUntrustedUIConfig::
    ~PrivacySandboxDialogUntrustedUIConfig() = default;

PrivacySandboxDialogUntrustedUI::PrivacySandboxDialogUntrustedUI(
    content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* untrusted_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUIUntrustedPrivacySandboxDialogURL);

  untrusted_source->AddFrameAncestor(
      GURL(chrome::kChromeUIPrivacySandboxDialogURL));
}

PrivacySandboxDialogUntrustedUI::~PrivacySandboxDialogUntrustedUI() = default;
