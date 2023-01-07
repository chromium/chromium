// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_NAMED_MESSAGE_PORT_CONNECTOR_CAST_H_
#define CHROMECAST_BROWSER_NAMED_MESSAGE_PORT_CONNECTOR_CAST_H_

#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/browser/cast_web_contents_observer.h"
#include "components/cast/named_message_port_connector/named_message_port_connector.h"

namespace chromecast {

// Injects and connects to NamedMessagePortConnector services into documents
// hosted by |cast_web_contents|.
class NamedMessagePortConnectorCast
    : public cast_api_bindings::NamedMessagePortConnector,
      public CastWebContentsObserver {
 public:
  // |cast_web_contents|: The CastWebContents which will receive port connection
  //                      services. Must outlive |this|.
  explicit NamedMessagePortConnectorCast(
      chromecast::CastWebContents* cast_web_contents);
  ~NamedMessagePortConnectorCast() override;

  NamedMessagePortConnectorCast(const NamedMessagePortConnectorCast&) = delete;
  void operator=(const NamedMessagePortConnectorCast&) = delete;

 private:
  // Sends a connection message to |cast_web_contents_|.
  // Should be invoked when |cast_web_contents| has finished loading a page,
  // and its main frame finished loading with no further pending navigations.
  void OnPageLoaded();

  // CastWebContentsObserver implementation.
  void PageStateChanged(PageState page_state) override;

  chromecast::CastWebContents* cast_web_contents_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_NAMED_MESSAGE_PORT_CONNECTOR_CAST_H_
