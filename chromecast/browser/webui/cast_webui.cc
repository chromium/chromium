// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webui/cast_webui.h"

#include "base/command_line.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/browser/webui/cast_resource_data_source.h"
#include "chromecast/browser/webui/cast_webui_message_handler.h"
#include "chromecast/browser/webui/constants.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/bindings_policy.h"

namespace chromecast {

CastWebUI::CastWebUI(content::WebUI* webui,
                     const std::string& host,
                     mojom::WebUiClient* client)
    : WebUIController(webui),
      web_contents_(webui->GetWebContents()),
      browser_context_(web_contents_->GetBrowserContext()) {
  DCHECK(web_contents_);
  DCHECK(browser_context_);
  weak_this_ = weak_factory_.GetWeakPtr();
  webui->SetBindings(content::kWebUIBindingsPolicySet);
  auto cast_resources =
      std::make_unique<CastResourceDataSource>(host, true /* for_webui */);
  client->CreateController(host, web_ui_.BindNewPipeAndPassRemote(),
                           cast_resources->BindNewPipeAndPassReceiver());
  if (host == kCastWebUIHomeHost) {
    cast_resources->OverrideContentSecurityPolicyChildSrc(
        "frame-src https://*.google.com;");
    cast_resources->DisableDenyXFrameOptions();
  } else if (host == kCastWebUIForceUpdateHost) {
    const std::string candidate_remote_url =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kForceUpdateRemoteUrl);
    cast_resources->OverrideContentSecurityPolicyChildSrc(
        "frame-src https://*.google.com " + candidate_remote_url);
    cast_resources->DisableDenyXFrameOptions();
  }
  content::URLDataSource::Add(browser_context_, std::move(cast_resources));
  auto message_handler = std::make_unique<CastWebUIMessageHandler>();
  message_handler_ = message_handler.get();
  webui->AddMessageHandler(std::move(message_handler));
}

CastWebUI::~CastWebUI() {}

void CastWebUI::InvokeCallback(const std::string& message,
                               const base::Value::List& args) {
  if (message_callbacks_.count(message) == 0) {
    return;
  }
  message_callbacks_[message]->OnMessage(args.Clone());
}

void CastWebUI::RegisterMessageCallback(
    const std::string& message,
    mojo::PendingRemote<mojom::MessageCallback> callback) {
  message_callbacks_.emplace(message, std::move(callback));
  web_ui()->RegisterMessageCallback(
      message,
      base::BindRepeating(&CastWebUI::InvokeCallback, weak_this_, message));
}

void CastWebUI::CallJavascriptFunction(const std::string& function,
                                       base::Value::List args) {
  message_handler_->CallJavascriptFunction(
      function, std::vector<base::ValueView>(args.begin(), args.end()));
}

}  // namespace chromecast
