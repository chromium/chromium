// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BINDINGS_NAMED_MESSAGE_PORT_CONNECTOR_CAST_H_
#define CHROMECAST_BINDINGS_NAMED_MESSAGE_PORT_CONNECTOR_CAST_H_

#include "components/cast/named_message_port_connector/named_message_port_connector.h"

namespace cast_api_bindings {
class Manager;
}

namespace chromecast {
class CastWebContents;

namespace bindings {

// Injects and connects to NamedMessagePortConnector services into documents
// hosted by |cast_web_contents|.
class NamedMessagePortConnectorCast
    : public cast_api_bindings::NamedMessagePortConnector {
 public:
  // |cast_web_contents|: The CastWebContents which will receive port connection
  //                      services.
  // |bindings_manager|: The BindingsManager instance that handles script
  //                     injection on |cast_web_contents|.
  // Both arguments must outlive |this|.
  NamedMessagePortConnectorCast(chromecast::CastWebContents* cast_web_contents,
                                cast_api_bindings::Manager* bindings_manager);
  ~NamedMessagePortConnectorCast() override;

  NamedMessagePortConnectorCast(const NamedMessagePortConnectorCast&) = delete;
  void operator=(const NamedMessagePortConnectorCast&) = delete;

  // Sends a connection message to |cast_web_contents_|.
  // Should be invoked when |cast_web_contents| has finished loading a page.
  void OnPageLoaded();

 private:
  chromecast::CastWebContents* cast_web_contents_;
  cast_api_bindings::Manager* bindings_manager_;
};

}  // namespace bindings
}  // namespace chromecast

#endif  // CHROMECAST_BINDINGS_NAMED_MESSAGE_PORT_CONNECTOR_CAST_H_
