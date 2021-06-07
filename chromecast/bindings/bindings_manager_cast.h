// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_H_
#define CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_H_

#include "chromecast/bindings/bindings_manager.h"
#include "chromecast/bindings/public/mojom/api_bindings.mojom.h"
#include "chromecast/browser/cast_web_contents.h"
#include "components/cast/api_bindings/manager.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromecast {
namespace bindings {

class NamedMessagePortConnectorCast;

// Implements the CastOS BindingsManager.
class BindingsManagerCast : public BindingsManager,
                            public CastWebContents::Observer,
                            public chromecast::mojom::ApiBindings {
 public:
  explicit BindingsManagerCast(chromecast::CastWebContents* cast_web_contents);
  ~BindingsManagerCast() override;

  BindingsManagerCast(const BindingsManagerCast&) = delete;
  void operator=(const BindingsManagerCast&) = delete;

  // Creates an mojo::PendingRemote, binds it to |this| and returns it.
  // At most one bound remote can exist at the same time.
  mojo::PendingRemote<mojom::ApiBindings> CreateRemote();

  // BindingsManager implementation.
  void AddBinding(base::StringPiece binding_name,
                  base::StringPiece binding_script) override;

 private:
  void OnClientDisconnected();

  // CastWebContents::Observer implementation.
  void OnPageStateChanged(CastWebContents* cast_web_contents) override;

  // chromecast::mojom::ApiBindings implementation.
  void GetAll(GetAllCallback callback) override;
  void Connect(const std::string& port_name,
               blink::MessagePortDescriptor port) override;

  chromecast::CastWebContents* cast_web_contents_;
  std::unique_ptr<NamedMessagePortConnectorCast> port_connector_;

  // Stores all bindings, keyed on the string-based IDs provided by the
  // ApiBindings interface.
  std::map<std::string, std::string> bindings_;

  mojo::Receiver<mojom::ApiBindings> receiver_{this};
};

}  // namespace bindings
}  // namespace chromecast

#endif  // CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_H_
