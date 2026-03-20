// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/content_annotator_internals/content_annotator_internals_ui.h"

#include <memory>

#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/content_annotator_internals/content_annotator_internals_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/content_annotator_internals_resources.h"
#include "chrome/grit/content_annotator_internals_resources_map.h"
#include "components/accessibility_annotator/core/logging/accessibility_annotator_internals.mojom.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_util.h"

namespace content_annotator_internals {

ContentAnnotatorInternalsUI::ContentAnnotatorInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://content-annotator-internals source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui),
      chrome::kChromeUIContentAnnotatorInternalsHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      source, kContentAnnotatorInternalsResources,
      IDR_CONTENT_ANNOTATOR_INTERNALS_CONTENT_ANNOTATOR_INTERNALS_HTML);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      base::StrCat({webui::kDefaultTrustedTypesPolicies,
                    " content-annotator-internals;"}));
}

void ContentAnnotatorInternalsUI::BindInterface(
    mojo::PendingReceiver<
        accessibility_annotator_internals::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ContentAnnotatorInternalsUI::CreatePageHandler(
    mojo::PendingRemote<accessibility_annotator_internals::mojom::Page> page,
    mojo::PendingReceiver<accessibility_annotator_internals::mojom::PageHandler>
        receiver) {
  page_handler_ = std::make_unique<ContentAnnotatorInternalsPageHandler>(
      std::move(receiver), std::move(page), Profile::FromWebUI(web_ui()));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ContentAnnotatorInternalsUI)
ContentAnnotatorInternalsUI::~ContentAnnotatorInternalsUI() = default;

}  // namespace content_annotator_internals
