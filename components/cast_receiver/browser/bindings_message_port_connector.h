// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_BINDINGS_MESSAGE_PORT_CONNECTOR_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_BINDINGS_MESSAGE_PORT_CONNECTOR_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/cast/named_message_port_connector/named_message_port_connector.h"
#include "components/cast_receiver/browser/page_state_observer.h"
#include "components/on_load_script_injector/browser/on_load_script_injector_host.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

namespace cast_api_bindings {
class MessagePort;
}  // namespace cast_api_bindings

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace cast_receiver {

// Injects and connects NamedMessagePortConnector services into documents
// hosted by |web_contents|.
class BindingsMessagePortConnector
    : public cast_api_bindings::NamedMessagePortConnector,
      public content::WebContentsObserver,
      public PageStateObserver {
 public:
  class Client {
   public:
    struct ApiBinding {
      // Script to execute before the load of a web document. |script| uses
      // UTF-8 encoding.
      std::string script;
    };

    virtual ~Client();

    // Gets the list of bindings to early-inject into the page at load time.
    // The returned ApiBindings must be evaluated in order.
    virtual std::vector<ApiBinding> GetAllBindings() = 0;

    // Should be invoked when a connecting a named MessagePort to a native
    // bindings backend.
    // |port_name| is a string-based ID. It is used to locate corresponding
    // native bindings backend.
    // |port| is one end of a paired message channel. It can be deserilized
    // to bind a blink::WebMessagePort to perform bi-directional communication.
    virtual void Connect(const std::string& port_name,
                         blink::MessagePortDescriptor port) = 0;

    // Called when a non-recoverable error has occurred.
    virtual void OnError() = 0;
  };

  BindingsMessagePortConnector(content::WebContents* web_contents,
                               Client& client);
  ~BindingsMessagePortConnector() override;

  BindingsMessagePortConnector(const BindingsMessagePortConnector&) = delete;
  void operator=(const BindingsMessagePortConnector&) = delete;

  // Connect to the underlying bindings service and apply all bindings of which
  // the client is aware.
  void ConnectToBindingsService();

 private:
  // Adds a new binding.
  void AddBeforeLoadJavaScript(uint64_t id, std::string_view script);

  // Callback for RegisterPortHandler().
  void OnPortConnected(std::string_view port_name,
                       std::unique_ptr<cast_api_bindings::MessagePort> port);

  // PageStateObserver implementation:
  void OnPageLoadComplete() override;

  // content::WebContentsObserver implementation:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

  raw_ref<Client> const client_;

  on_load_script_injector::OnLoadScriptInjectorHost<uint64_t> script_injector_;

  raw_ptr<content::NavigationHandle> active_navigation_ = nullptr;

  base::WeakPtrFactory<BindingsMessagePortConnector> weak_factory_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_BINDINGS_MESSAGE_PORT_CONNECTOR_H_
