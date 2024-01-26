// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include "base/check.h"
#include "base/logging.h"
#include "components/exo/wayland/compatibility_test/wayland_client_registry.h"

namespace exo {
namespace wayland {
namespace compatibility {
namespace test {

WaylandClientRegistry::WaylandClientRegistry(wl_display* display)
    : registry_(wl_display_get_registry(display)) {
  static wl_registry_listener registry_listener = {
      &WaylandClientRegistry::Add,
      &WaylandClientRegistry::Remove,
  };
  int err = wl_registry_add_listener(registry_.get(), &registry_listener, this);
  DCHECK(err == 0);
}

WaylandClientRegistry::~WaylandClientRegistry() = default;

std::optional<WaylandClientRegistry::Entry> WaylandClientRegistry::GetEntry(
    const char* interface_name) const noexcept {
  DCHECK(registry_);
  if (!registry_)
    return {};

  const auto it = globals_.find(interface_name);
  if (it == globals_.end())
    return {};

  return it->second;
}

bool WaylandClientRegistry::Has(const char* interface_name,
                                uint32_t client_version) const noexcept {
  const auto entry = GetEntry(interface_name);
  return entry ? entry->server_version >= client_version : false;
}

void* WaylandClientRegistry::Bind(const char* interface_name,
                                  const struct wl_interface* protocol_interface,
                                  uint32_t protocol_version,
                                  uint32_t client_version) noexcept {
  const auto entry = GetEntry(interface_name);

  if (!entry || entry->server_version < client_version)
    return nullptr;

  // This code should be built from the same sources. While the newest
  // versions of the protocol may not be used by the server, the server
  // shouldn't be claiming support for an unexpected version.
  DCHECK(entry->server_version <= protocol_version);

  return wl_registry_bind(registry_.get(), entry->name, protocol_interface,
                          client_version);
}

void WaylandClientRegistry::Add(void* context,
                                wl_registry*,
                                uint32_t name,
                                const char* interface,
                                uint32_t version) noexcept {
  WaylandClientRegistry* registry =
      static_cast<WaylandClientRegistry*>(context);
  registry->globals_.emplace(interface,
                             WaylandClientRegistry::Entry{name, version});
}

void WaylandClientRegistry::Remove(void*,
                                   wl_registry*,
                                   uint32_t name) noexcept {
  LOG(ERROR) << "Unexpected global remove of id " << name;
  DCHECK(false);
}

}  // namespace test
}  // namespace compatibility
}  // namespace wayland
}  // namespace exo
