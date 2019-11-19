// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BINDINGS_BINDINGS_MANAGER_FUCHSIA_H_
#define CHROMECAST_BINDINGS_BINDINGS_MANAGER_FUCHSIA_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "chromecast/bindings/bindings_manager.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"

namespace chromecast {
namespace bindings {

// Implements the BindingsManager as a ApiBindings FIDL service.
class BindingsManagerFuchsia : public chromium::cast::ApiBindings,
                               public BindingsManager {
 public:
  BindingsManagerFuchsia();
  ~BindingsManagerFuchsia() override;

  // BindingsManager implementation:
  void AddBinding(base::StringPiece binding_name,
                  base::StringPiece binding_script) override;

 protected:
  // chromium::cast::ApiBindings implementation:
  void GetAll(GetAllCallback callback) override;
  void Connect(
      std::string port_name,
      fidl::InterfaceHandle<::fuchsia::web::MessagePort> message_port) override;

 private:
  // Stores all bindings, keyed on the string-based IDs provided by the
  // ApiBindings interface.
  std::map<std::string, fuchsia::mem::Buffer> bindings_;

  DISALLOW_COPY_AND_ASSIGN(BindingsManagerFuchsia);
};

}  // namespace bindings
}  // namespace chromecast

#endif  // CHROMECAST_BINDINGS_BINDINGS_MANAGER_FUCHSIA_H_
