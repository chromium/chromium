// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_TEST_WLCS_WLCS_HELPERS_H_
#define COMPONENTS_EXO_WAYLAND_TEST_WLCS_WLCS_HELPERS_H_

#include <wayland-client-core.h>
#include <wayland-server-core.h>

#include "base/at_exit.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/exo/wayland/fuzzer/server_environment.h"

namespace exo::wayland {
class Server;
}

namespace ui::test {
class EventGenerator;
}

namespace exo::wlcs {

// WlcsInitializer is an object that simply calls some static initialization
// code in its constructor. This allows us to use member initialization order to
// guaranteed the static initialization occurs at the correct time.
class WlcsInitializer {
 public:
  WlcsInitializer();
};

// WlcsEnvironment holds the handles to all the runtime objects used across
// tests (i.e. that outlive the test).
//
// This class has a global singleton instance accessible via Get().
class WlcsEnvironment {
 public:
  static WlcsEnvironment& Get();

  base::AtExitManager exit_manager;
  WlcsInitializer initializer;
  wayland_fuzzer::ServerEnvironment env;

 private:
  WlcsEnvironment();
  ~WlcsEnvironment();
};

// Scoped object which allows WLCS to create/destroy exo's servers based on the
// lifetime of this object.
//
// Additionally, this class keeps track of wayland clients created for its
// server to facilitate associating wl_proxy objects with their server-side
// resource.
class ScopedWlcsServer {
 public:
  explicit ScopedWlcsServer();
  ~ScopedWlcsServer();

  wayland::Server* Get() const;

  // Add a wayland client, returning an FD which connects to the server, or -1
  // on error.
  //
  // Additionally, clients created this way will be tracked, allowing the user
  // to call ObjectToResource() to get server-side handles for this client's
  // objects.
  int AddClient();

  wl_resource* ProxyToResource(struct wl_display* client_display,
                               wl_proxy* client_side_proxy) const;

  // Returns the server-side wl_resource associated with the provided
  // client_object (all client-side wayland objects are opaquely backed by
  // wl_proxy).
  template <typename Obj>
  wl_resource* ObjectToResource(struct wl_display* client_display,
                                Obj client_object) const {
    // Wayland interfaces are basically transparent types that get created by
    // casting a wl_proxy to the relevant struct name, so it is safe to undo
    // that with reinterpret_cast().
    return ProxyToResource(client_display,
                           reinterpret_cast<wl_proxy*>(client_object));
  }

  // Convenience method for generating ui events. This method will run the given
  // |invocation| on the UI thread with the correct event generator, in a
  // blocking fashion.
  //
  // TODO(b/276660692): prefer ui_controls over ui::test::EventGenerator due to
  // the former's increased coverage. We didn't do so for now due to some
  // missing APIs.
  void GenerateEvent(
      base::OnceCallback<void(ui::test::EventGenerator&)> invocation);

 private:
  base::flat_map<int, raw_ptr<wl_client, CtnExperimental>> fd_to_client_;

  std::unique_ptr<ui::test::EventGenerator> evg_;
};

}  // namespace exo::wlcs

#endif  // COMPONENTS_EXO_WAYLAND_TEST_WLCS_WLCS_HELPERS_H_
