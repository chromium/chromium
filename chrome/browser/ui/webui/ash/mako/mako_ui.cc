// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/input_method/editor_mediator.h"
#include "chrome/browser/ui/webui/ash/mako/mako_source.h"
#include "chrome/browser/ui/webui/ash/mako/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace ash {

MakoUntrustedUIConfig::MakoUntrustedUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme, ash::kChromeUIMakoHost) {}

MakoUntrustedUIConfig::~MakoUntrustedUIConfig() = default;

std::unique_ptr<content::WebUIController>
MakoUntrustedUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                             const GURL& url) {
  return std::make_unique<MakoUntrustedUI>(web_ui);
}

bool MakoUntrustedUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kOrca);
}

MakoUntrustedUI::MakoUntrustedUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  CHECK(base::FeatureList::IsEnabled(features::kOrca));
  content::URLDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                              std::make_unique<MakoSource>());
}
MakoUntrustedUI::~MakoUntrustedUI() = default;

void MakoUntrustedUI::BindInterface(
    mojo::PendingReceiver<input_method::mojom::EditorInstance> receiver) {
  input_method::EditorMediator::Get()->BindEditorInstance(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(MakoUntrustedUI)

MakoPageHandler::MakoPageHandler() {
  // TODO(b/289859230): Construct MakoUntrustedUI and show it to the user. Save
  //   a ref to the constructed view to allow for closing it at a later time.
  NOTIMPLEMENTED_LOG_ONCE();
}

MakoPageHandler::~MakoPageHandler() = default;

void MakoPageHandler::CloseUI() {
  // TODO(b/289859230): Use the ref saved from construction to close the webui.
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace ash
