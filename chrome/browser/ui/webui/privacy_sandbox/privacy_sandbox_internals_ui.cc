// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_ui.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/feature_list.h"
#include "chrome/browser/ui/webui/privacy_sandbox/private_state_tokens/private_state_tokens_handler.h"
#include "chrome/browser/ui/webui/privacy_sandbox/related_website_sets/related_website_sets_handler.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/browser/url_data_source.h"
#endif

#include "base/json/json_writer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
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

namespace privacy_sandbox_internals {

using ::privacy_sandbox_internals::mojom::Page;
using ::privacy_sandbox_internals::mojom::PageHandler;
#if !BUILDFLAG(IS_ANDROID)
using ::private_state_tokens::mojom::PrivateStateTokensPageHandler;
using ::related_website_sets::mojom::RelatedWebsiteSetsPageHandler;
#endif

bool PrivacySandboxInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      privacy_sandbox::kPrivacySandboxInternalsDevUI);
}

PrivacySandboxInternalsUI::PrivacySandboxInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIPrivacySandboxInternalsHost);
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kPrivacySandboxInternalsResources,
                      kPrivacySandboxInternalsResourcesSize),
      IDR_PRIVACY_SANDBOX_INTERNALS_INDEX_HTML);

#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(privacy_sandbox::kRelatedWebsiteSetsDevUI)) {
    content::URLDataSource::Add(
        Profile::FromWebUI(web_ui),
        std::make_unique<SanitizedImageSource>(Profile::FromWebUI(web_ui)));
    source->AddResourcePath("related-website-sets",
                            IDR_RELATED_WEBSITE_SETS_RELATED_WEBSITE_SETS_HTML);
  }
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivateStateTokensDevUI)) {
    source->AddResourcePath("private-state-tokens",
                            IDR_PRIVATE_STATE_TOKENS_PRIVATE_STATE_TOKENS_HTML);
  }

  static constexpr webui::LocalizedString pstDevUiPageStrings[] = {
      // Localized Strings
      {"privateStateTokensDescriptionLabel",
       IDS_PRIVATE_STATE_TOKENS_DESCRIPTION_LABEL},
      {"privateStateTokensHeadingLabel",
       IDS_PRIVATE_STATE_TOKENS_HEADING_LABEL},
      {"privateStateTokensExternalLinkLabel", IDS_LEARN_MORE}};

  source->AddLocalizedStrings(pstDevUiPageStrings);

#endif
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

#if !BUILDFLAG(IS_ANDROID)
void PrivacySandboxInternalsUI::BindInterface(
    mojo::PendingReceiver<
        related_website_sets::mojom::RelatedWebsiteSetsPageHandler> receiver) {
  if (base::FeatureList::IsEnabled(privacy_sandbox::kRelatedWebsiteSetsDevUI)) {
    related_website_sets_handler_ = std::make_unique<RelatedWebsiteSetsHandler>(
        web_ui(), std::move(receiver));
  }
}

void PrivacySandboxInternalsUI::BindInterface(
    mojo::PendingReceiver<
        private_state_tokens::mojom::PrivateStateTokensPageHandler> receiver) {
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivateStateTokensDevUI)) {
    private_state_tokens_handler_ = std::make_unique<PrivateStateTokensHandler>(
        web_ui(), std::move(receiver));
  }
}
#endif

}  // namespace privacy_sandbox_internals
