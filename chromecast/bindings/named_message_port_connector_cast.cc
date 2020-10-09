// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/bindings/named_message_port_connector_cast.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/browser/cast_web_contents.h"
#include "components/cast/api_bindings/manager.h"
#include "components/cast/message_port/message_port_cast.h"
#include "components/cast/named_message_port_connector/grit/named_message_port_connector_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromecast {
namespace bindings {
namespace {

const char kNamedMessagePortConnectorBindingsId[] =
    "NAMED_MESSAGE_PORT_CONNECTOR";

}  // namespace

NamedMessagePortConnectorCast::NamedMessagePortConnectorCast(
    chromecast::CastWebContents* cast_web_contents,
    cast_api_bindings::Manager* bindings_manager)
    : cast_web_contents_(cast_web_contents),
      bindings_manager_(bindings_manager) {
  DCHECK(cast_web_contents_);
  DCHECK(bindings_manager_);

  // Register the port connection JS script for early injection.
  std::string bindings_script_string =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_PORT_CONNECTOR_JS);
  DCHECK(!bindings_script_string.empty())
      << "NamedMessagePortConnector resources not loaded.";
  bindings_manager_->AddBinding(kNamedMessagePortConnectorBindingsId,
                                bindings_script_string);
}

NamedMessagePortConnectorCast::~NamedMessagePortConnectorCast() = default;

void NamedMessagePortConnectorCast::OnPageLoaded() {
  // Send the port connection message to the page once it is loaded.
  std::string connect_message;
  std::unique_ptr<cast_api_bindings::MessagePort> port;
  GetConnectMessage(&connect_message, &port);

  std::vector<blink::WebMessagePort> ports;
  ports.push_back(
      cast_api_bindings::MessagePortCast::FromMessagePort(port.get())
          ->TakePort());
  cast_web_contents_->PostMessageToMainFrame("*", connect_message,
                                             std::move(ports));
}

}  // namespace bindings
}  // namespace chromecast
