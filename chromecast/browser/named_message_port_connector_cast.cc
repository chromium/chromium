// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/named_message_port_connector_cast.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/browser/cast_web_contents.h"
#include "components/cast/message_port/blink_message_port_adapter.h"
#include "components/cast/message_port/cast/message_port_cast.h"
#include "components/cast/named_message_port_connector/grit/named_message_port_connector_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromecast {
namespace {

constexpr uint64_t kNamedMessagePortConnectorBindingsId = 1000;

}  // namespace

NamedMessagePortConnectorCast::NamedMessagePortConnectorCast(
    chromecast::CastWebContents* cast_web_contents)
    : cast_web_contents_(cast_web_contents) {
  DCHECK(cast_web_contents_);

  CastWebContentsObserver::Observe(cast_web_contents_);

  // Register the port connection JS script for early injection.
  std::string bindings_script_string =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_PORT_CONNECTOR_JS);
  DCHECK(!bindings_script_string.empty())
      << "NamedMessagePortConnector resources not loaded.";

  cast_web_contents->AddBeforeLoadJavaScript(
      kNamedMessagePortConnectorBindingsId, bindings_script_string);
}

NamedMessagePortConnectorCast::~NamedMessagePortConnectorCast() {
  CastWebContentsObserver::Observe(nullptr);
}

void NamedMessagePortConnectorCast::OnPageLoaded() {
  // Send the port connection message to the page once it is loaded.
  std::string connect_message;
  std::unique_ptr<cast_api_bindings::MessagePort> port;
  GetConnectMessage(&connect_message, &port);

  std::vector<blink::WebMessagePort> ports;
  ports.push_back(
      cast_api_bindings::BlinkMessagePortAdapter::FromServerPlatformMessagePort(
          std::move(port)));
  cast_web_contents_->PostMessageToMainFrame("*", connect_message,
                                             std::move(ports));
}

void NamedMessagePortConnectorCast::PageStateChanged(PageState page_state) {
  switch (page_state) {
    case PageState::DESTROYED:
    case PageState::ERROR:
      CastWebContentsObserver::Observe(nullptr);
      cast_web_contents_ = nullptr;
      break;
    case PageState::LOADED:
      OnPageLoaded();
      break;
    case PageState::IDLE:
    case PageState::LOADING:
    case PageState::CLOSED:
      break;
  }
}

}  // namespace chromecast
