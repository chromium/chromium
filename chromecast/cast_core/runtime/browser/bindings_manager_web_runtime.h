// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_BINDINGS_MANAGER_WEB_RUNTIME_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_BINDINGS_MANAGER_WEB_RUNTIME_H_

#include <map>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/cast_core/runtime/browser/message_port_service.h"
#include "components/cast/api_bindings/manager.h"
#include "components/cast_receiver/browser/bindings_message_port_connector.h"
#include "components/cast_receiver/common/public/status.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

namespace chromecast {

class MessagePortService;

// This class will be initialized with a set of bindings received during
// application launch and injects them into the app's WebContents when the page
// loads. It then handles connecting PortConnector requests from those bindings
// by making requests to a MessagePort instance. There should be one instance of
// this class for a single WebContents.
class BindingsManagerWebRuntime final
    : public cast_api_bindings::Manager,
      public cast_receiver::BindingsMessagePortConnector::Client {
 public:
  // Handles callbacks for state changes in this object.
  class Client {
   public:
    virtual ~Client();

    // Called when a non-recoverable error occurs.
    virtual void OnError() = 0;
  };

  BindingsManagerWebRuntime(
      Client& client,
      std::unique_ptr<MessagePortService> message_port_service);
  ~BindingsManagerWebRuntime() override;

  BindingsManagerWebRuntime(const BindingsManagerWebRuntime&) = delete;
  BindingsManagerWebRuntime(BindingsManagerWebRuntime&&) = delete;
  BindingsManagerWebRuntime& operator=(const BindingsManagerWebRuntime&) =
      delete;
  BindingsManagerWebRuntime& operator=(BindingsManagerWebRuntime&&) = delete;

  void AddBinding(base::StringPiece binding_script);
  cast_receiver::Status HandleMessage(cast::web::Message message);

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
  void AddBinding(base::StringPiece binding_name,
                  base::StringPiece binding_script) override;

  int next_script_id_{0};

  // Stores all bindings, keyed on the string-based IDs provided by the
  // ApiBindings interface.
  std::map<std::string, std::string> bindings_;

  // Used to open a MessageChannel for connecting API bindings.
  std::unique_ptr<cast_receiver::BindingsMessagePortConnector>
      message_port_connector_;

  std::unique_ptr<MessagePortService> message_port_service_;

  base::raw_ref<Client> client_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_BINDINGS_MANAGER_WEB_RUNTIME_H_
