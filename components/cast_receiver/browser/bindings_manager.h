// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_BINDINGS_MANAGER_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_BINDINGS_MANAGER_H_

#include <map>
#include <memory>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/cast/api_bindings/manager.h"
#include "components/cast_receiver/browser/bindings_message_port_connector.h"
#include "components/cast_receiver/common/public/status.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

namespace cast_receiver {

class MessagePortService;

// This class will be initialized with a set of bindings received during
// application launch and injects them into the app's WebContents when the page
// loads. It then handles connecting PortConnector requests from those bindings
// by making requests to a MessagePort instance. There should be one instance of
// this class for a single WebContents.
class BindingsManager final : public cast_api_bindings::Manager,
                              public BindingsMessagePortConnector::Client {
 public:
  // Handles callbacks for state changes in this object.
  class Client {
   public:
    virtual ~Client();

    // Called when a non-recoverable error occurs.
    virtual void OnError() = 0;
  };

  // |client| and |message_port_service| are expected to persist for the
  // duration of this instance's lifetime.
  BindingsManager(Client& client, MessagePortService& message_port_service);
  ~BindingsManager() override;

  BindingsManager(const BindingsManager&) = delete;
  BindingsManager(BindingsManager&&) = delete;
  BindingsManager& operator=(const BindingsManager&) = delete;
  BindingsManager& operator=(BindingsManager&&) = delete;

  void AddBinding(std::string_view binding_script);

  // Configures the |message_port_connector_| for use with this |web_contents|
  // and connects it to the bindings service.
  void ConfigureWebContents(content::WebContents* web_contents);

 private:
  // BindingsMessagePortConnector::Client overrides.
  std::vector<ApiBinding> GetAllBindings() override;
  void Connect(const std::string& port_name,
               blink::MessagePortDescriptor port) override;
  void OnError() override;

  // cast_api_bindings::Manager overrides.
  void AddBinding(std::string_view binding_name,
                  std::string_view binding_script) override;

  int next_script_id_{0};

  // Stores all bindings, keyed on the string-based IDs provided by the
  // ApiBindings interface.
  std::map<std::string, std::string> bindings_;

  // Used to open a MessageChannel for connecting API bindings.
  std::unique_ptr<BindingsMessagePortConnector> message_port_connector_;

  raw_ref<MessagePortService> message_port_service_;

  raw_ref<Client> client_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_BINDINGS_MANAGER_H_
