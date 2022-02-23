// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_BINDINGS_MANAGER_WEB_RUNTIME_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_BINDINGS_MANAGER_WEB_RUNTIME_H_

#include <map>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/bindings/public/mojom/api_bindings.mojom.h"
#include "chromecast/cast_core/runtime/browser/message_port_service.h"
#include "components/cast/api_bindings/manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/cast_core/public/src/proto/v2/core_message_port_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/web/message_channel.pb.h"

namespace chromecast {

// This class will be initialized with a set of bindings received over gRPC and
// will inject them into the app's CastWebContents when the page loads.  It then
// handles connecting PortConnector requests from those bindings by making gRPC
// ApiBindings requests to Cast Core.  There should be one instance of this
// class for a single CastWebContents.
class BindingsManagerWebRuntime final : public cast_api_bindings::Manager,
                                        public chromecast::mojom::ApiBindings {
 public:
  // |cast_web_contents|, |grpc_cq|, and |core_app_stub| all need to outlive
  // |this|.
  BindingsManagerWebRuntime(
      cast::v2::CoreMessagePortApplicationServiceStub* core_app_stub);
  ~BindingsManagerWebRuntime() override;

  BindingsManagerWebRuntime(const BindingsManagerWebRuntime&) = delete;
  BindingsManagerWebRuntime(BindingsManagerWebRuntime&&) = delete;
  BindingsManagerWebRuntime& operator=(const BindingsManagerWebRuntime&) =
      delete;
  BindingsManagerWebRuntime& operator=(BindingsManagerWebRuntime&&) = delete;

  void AddBinding(base::StringPiece binding_script);
  cast::utils::GrpcStatusOr<cast::web::MessagePortStatus> HandleMessage(
      cast::web::Message message);

  // Returns a mojo::PendingRemote bound to |this|.
  // At most one bound remote can exist at the same time.
  mojo::PendingRemote<mojom::ApiBindings> CreateRemote();

 private:
  // Callback invoked when client of mojom::ApiBindings disconnects.
  void OnMojoClientDisconnected();

  // cast_api_bindings::Manager overrides.
  void AddBinding(base::StringPiece binding_name,
                  base::StringPiece binding_script) override;

  // chromecast::mojom::ApiBindings implementation.
  void GetAll(GetAllCallback callback) override;
  void Connect(const std::string& port_name,
               blink::MessagePortDescriptor port) override;

  int next_script_id_{0};
  // Stores all bindings, keyed on the string-based IDs provided by the
  // ApiBindings interface.
  std::map<std::string, std::string> bindings_;
  mojo::Receiver<mojom::ApiBindings> receiver_{this};

  MessagePortService message_port_service_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_BINDINGS_MANAGER_WEB_RUNTIME_H_
