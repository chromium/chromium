// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_ui.h"

#include "base/json/json_writer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_handler.h"
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

#if !BUILDFLAG(IS_ANDROID)
#include "base/feature_list.h"
#include "chrome/browser/ui/webui/privacy_sandbox/related_website_sets/related_website_sets_handler.h"
#include "chrome/browser/ui/webui/sanitized_image/sanitized_image_source.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/browser/url_data_source.h"
#endif

namespace privacy_sandbox_internals {

using ::privacy_sandbox_internals::mojom::Page;
using ::privacy_sandbox_internals::mojom::PageHandler;
#if !BUILDFLAG(IS_ANDROID)
using ::related_website_sets::mojom::RelatedWebsiteSetsPageHandler;
#endif

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

void PrivacySandboxInternalsUI::BindInterface(
    mojo::PendingReceiver<privacy_sandbox_internals::mojom::PageHandler>
        receiver) {
  handler_ = std::make_unique<PrivacySandboxInternalsHandler>(
      Profile::FromBrowserContext(
          web_ui()->GetWebContents()->GetBrowserContext()),
      std::move(receiver));
}

#if !BUILDFLAG(IS_ANDROID)
void PrivacySandboxInternalsUI::BindInterface(
    mojo::PendingReceiver<
        related_website_sets::mojom::RelatedWebsiteSetsPageHandler> receiver) {
  if (base::FeatureList::IsEnabled(privacy_sandbox::kRelatedWebsiteSetsDevUI)) {
    related_website_sets_handler_ = std::make_unique<RelatedWebsiteSetsHandler>(
        web_ui(), std::move(receiver));
  }
}
#endif

}  // namespace privacy_sandbox_internals
