// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_ui.h"
#include "base/json/json_writer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/privacy_sandbox_internals_resources.h"
#include "chrome/grit/privacy_sandbox_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "privacy_sandbox_internals_ui.h"

namespace privacy_sandbox_internals {
using ::privacy_sandbox_internals::mojom::Page;
using ::privacy_sandbox_internals::mojom::PageHandler;

PrivacySandboxInternalsUI::PrivacySandboxInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIPrivacySandboxInternalsHost);
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kPrivacySandboxInternalsResources,
                      kPrivacySandboxInternalsResourcesSize),
      IDR_PRIVACY_SANDBOX_INTERNALS_INDEX_HTML);
}

PrivacySandboxInternalsUI::~PrivacySandboxInternalsUI() {}

WEB_UI_CONTROLLER_TYPE_IMPL(PrivacySandboxInternalsUI)

void PrivacySandboxInternalsUI::BindInterface(
    mojo::PendingReceiver<privacy_sandbox_internals::mojom::PageHandler>
        receiver) {
  handler_ = std::make_unique<PrivacySandboxInternalsHandler>(
      Profile::FromBrowserContext(
          web_ui()->GetWebContents()->GetBrowserContext()),
      std::move(receiver));
}

}  // namespace privacy_sandbox_internals
