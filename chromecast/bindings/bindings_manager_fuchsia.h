// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BINDINGS_BINDINGS_MANAGER_FUCHSIA_H_
#define CHROMECAST_BINDINGS_BINDINGS_MANAGER_FUCHSIA_H_

#include <chromium/cast/cpp/fidl.h>
#include <fuchsia/mem/cpp/fidl.h>

#include <list>
#include <map>
#include <string>
#include <string_view>

#include "chromecast/bindings/bindings_manager.h"

namespace chromecast {
namespace bindings {

// Implements the BindingsManager as a ApiBindings FIDL service.
class BindingsManagerFuchsia : public chromium::cast::ApiBindings,
                               public BindingsManager {
 public:
  BindingsManagerFuchsia();

  BindingsManagerFuchsia(const BindingsManagerFuchsia&) = delete;
  BindingsManagerFuchsia& operator=(const BindingsManagerFuchsia&) = delete;

  ~BindingsManagerFuchsia() override;

  // BindingsManager implementation:
  void AddBinding(std::string_view binding_name,
                  std::string_view binding_script) override;

 protected:
  // chromium::cast::ApiBindings implementation:
  void GetAll(GetAllCallback callback) override;
  void Connect(
      std::string port_name,
      fidl::InterfaceHandle<::fuchsia::web::MessagePort> message_port) override;

 private:
  // Stores all bindings, keyed on the string-based IDs provided by the
  // ApiBindings interface. Bindings are stored in the order they are added
  // because evaluation order matters when one depends on another.
  std::list<std::pair<std::string, fuchsia::mem::Buffer>> bindings_;
};

}  // namespace bindings
}  // namespace chromecast

#endif  // CHROMECAST_BINDINGS_BINDINGS_MANAGER_FUCHSIA_H_
