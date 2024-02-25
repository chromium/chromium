// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_COMPATIBILITY_TEST_WAYLAND_CLIENT_REGISTRY_H_
#define COMPONENTS_EXO_WAYLAND_COMPATIBILITY_TEST_WAYLAND_CLIENT_REGISTRY_H_

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "components/exo/wayland/compatibility_test/generated-wayland-client-helpers.h"

namespace exo {
namespace wayland {
namespace compatibility {
namespace test {

class WaylandClientRegistry {
 public:
  explicit WaylandClientRegistry(wl_display* display);
  ~WaylandClientRegistry();

  WaylandClientRegistry(const WaylandClientRegistry&) = delete;
  WaylandClientRegistry& operator=(const WaylandClientRegistry&) = delete;

  template <typename T>
  bool Has(uint32_t client_version) const noexcept;

  template <typename T>
  std::unique_ptr<T> Bind(uint32_t client_version) noexcept;

 private:
  static void Add(void*,
                  wl_registry*,
                  uint32_t name,
                  const char* interface,
                  uint32_t version) noexcept;
  static void Remove(void*, wl_registry*, uint32_t name) noexcept;

  struct Entry {
    uint32_t name;
    uint32_t server_version;
  };

  std::optional<Entry> GetEntry(const char* interface_name) const noexcept;
  bool Has(const char* interface_name, uint32_t client_version) const noexcept;
  void* Bind(const char* interface_name,
             const struct wl_interface* protocol_interface,
             uint32_t protocol_version,
             uint32_t client_version) noexcept;

  std::unique_ptr<wl_registry> registry_;
  std::unordered_map<std::string, Entry> globals_;
};

template <typename T>
bool WaylandClientRegistry::Has(uint32_t client_version) const noexcept {
  using Descriptor = WaylandGlobalInterfaceDescriptor<T>;
  return Has(Descriptor::interface_name, client_version);
}

template <typename T>
std::unique_ptr<T> WaylandClientRegistry::Bind(
    uint32_t client_version) noexcept {
  using Descriptor = WaylandGlobalInterfaceDescriptor<T>;
  return std::unique_ptr<typename Descriptor::CType>(
      static_cast<typename Descriptor::CType*>(
          Bind(Descriptor::interface_name, Descriptor::protocol_interface,
               Descriptor::protocol_version, client_version)));
}

}  // namespace test
}  // namespace compatibility
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_COMPATIBILITY_TEST_WAYLAND_CLIENT_REGISTRY_H_
