// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_H_
#define CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_H_

#include <list>
#include <string_view>

#include "base/functional/callback.h"
#include "chromecast/bindings/bindings_manager.h"
#include "chromecast/bindings/public/mojom/api_bindings.mojom.h"
#include "components/cast/api_bindings/manager.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromecast {
namespace bindings {

// Implements the CastOS BindingsManager.
class BindingsManagerCast : public BindingsManager,
                            public chromecast::mojom::ApiBindings {
 public:
  // |cast_web_contents|: Used to inject bindings scripts into document early.
  //                      Must outlive |this|.
  BindingsManagerCast();
  ~BindingsManagerCast() override;

  BindingsManagerCast(const BindingsManagerCast&) = delete;
  void operator=(const BindingsManagerCast&) = delete;

  // Creates an mojo::PendingRemote, binds it to |this| and returns it.
  // At most one bound remote can exist at the same time.
  mojo::PendingRemote<mojom::ApiBindings> CreateRemote();

  // BindingsManager implementation.
  void AddBinding(std::string_view binding_name,
                  std::string_view binding_script) override;

 private:
  void OnClientDisconnected();

  // chromecast::mojom::ApiBindings implementation.
  void GetAll(GetAllCallback callback) override;
  void Connect(const std::string& port_name,
               blink::MessagePortDescriptor port) override;

  // Stores all bindings, keyed on the string-based IDs provided by the
  // ApiBindings interface. Bindings are stored in the order they are added
  // because evaluation order matters when one depends on another.
  std::list<std::pair<std::string, std::string>> bindings_;

  mojo::Receiver<mojom::ApiBindings> receiver_{this};
};

}  // namespace bindings
}  // namespace chromecast

#endif  // CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_H_
