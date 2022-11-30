// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webui/cast_webui_controller_factory.h"

#include <algorithm>
#include <mutex>

#include "base/containers/contains.h"
#include "chromecast/browser/webui/cast_resource_data_source.h"
#include "chromecast/browser/webui/cast_webui.h"
#include "chromecast/browser/webui/constants.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "url/gurl.h"

namespace chromecast {

CastWebUiControllerFactory::CastWebUiControllerFactory(
    mojo::PendingRemote<mojom::WebUiClient> client,
    const std::vector<std::string>& hosts)
    : client_(std::move(client)), hosts_(hosts) {
  DCHECK(client_);
}

CastWebUiControllerFactory::~CastWebUiControllerFactory() = default;

content::WebUI::TypeID CastWebUiControllerFactory::GetWebUIType(
    content::BrowserContext* browser_context,
    const GURL& url) {
  if (base::Contains(hosts_, url.host())) {
    return const_cast<CastWebUiControllerFactory*>(this);
  }
  return content::WebUI::kNoWebUI;
}

bool CastWebUiControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return GetWebUIType(browser_context, url) != content::WebUI::kNoWebUI;
}

std::unique_ptr<content::WebUIController>
CastWebUiControllerFactory::CreateWebUIControllerForURL(content::WebUI* web_ui,
                                                        const GURL& url) {
  static std::once_flag flag;
  std::call_once(flag, [this, web_ui] {
    auto cast_resources = std::make_unique<CastResourceDataSource>(
        kCastWebUIResourceHost, false /* for_webui */);
    client_->CreateResources(cast_resources->GetSource(),
                             cast_resources->BindNewPipeAndPassReceiver());
    content::URLDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                std::move(cast_resources));
  });

  return CastWebUI::Create(web_ui, url.host(), client_.get());
}

}  // namespace chromecast
