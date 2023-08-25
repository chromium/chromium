// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/wlcs/wlcs_helpers.h"
#include <memory>

#include "ash/shell.h"
#include "base/command_line.h"
#include "base/posix/unix_domain_socket.h"
#include "base/test/test_timeouts.h"
#include "components/exo/wayland/server.h"
#include "ui/events/test/event_generator.h"

namespace exo::wlcs {

WlcsInitializer::WlcsInitializer() {
  DCHECK(base::CommandLine::Init(0, nullptr));
  // TODO(272136757): Refactor initialization so that we don;t need to do this.
  base::CommandLine::ForCurrentProcess()->DetachFromCurrentSequence();
  TestTimeouts::Initialize();
}

// static
WlcsEnvironment& WlcsEnvironment::Get() {
  static WlcsEnvironment instance;
  return instance;
}

WlcsEnvironment::WlcsEnvironment() = default;

WlcsEnvironment::~WlcsEnvironment() {
  // The AtExitManager outlives the ui thread, which is needed to correctly
  // shut-down some components that have weak pointers.
  //
  // In our case it's fine to ignore that because the manager has static
  // lifetime so not shutting-down just means we leave cleanup to the process
  // exit.
  exit_manager.DisableAllAtExitManagers();
}

ScopedWlcsServer::ScopedWlcsServer() {
  WlcsEnvironment::Get().env.SetUp();
  WlcsEnvironment::Get().env.RunOnUiThreadBlocking(base::BindOnce(
      [](std::unique_ptr<ui::test::EventGenerator>* out_evg_up) {
        *out_evg_up = std::make_unique<ui::test::EventGenerator>(
            ash::Shell::GetPrimaryRootWindow());
      },
      &evg_));
}

ScopedWlcsServer::~ScopedWlcsServer() {
  WlcsEnvironment::Get().env.TearDown();
}

wayland::Server* ScopedWlcsServer::Get() const {
  return WlcsEnvironment::Get().env.wayland_server();
}

int ScopedWlcsServer::AddClient() {
  base::ScopedFD server_fd, client_fd;
  if (!base::CreateSocketPair(&server_fd, &client_fd)) {
    return -1;
  }

  struct wl_client* client =
      wl_client_create(Get()->GetWaylandDisplay(), server_fd.get());
  if (!client) {
    return -1;
  }

  // If the client was created successfully wayland will close the socket for
  // us, so we can release our handle.
  std::ignore = server_fd.release();
  fd_to_client_.insert({client_fd.get(), client});
  return client_fd.release();
}

wl_resource* ScopedWlcsServer::ProxyToResource(
    struct wl_display* client_display,
    wl_proxy* client_side_proxy) const {
  int client_fd = wl_display_get_fd(client_display);
  int obj_id = wl_proxy_get_id(client_side_proxy);
  return wl_client_get_object(fd_to_client_.at(client_fd), obj_id);
}

void ScopedWlcsServer::GenerateEvent(
    base::OnceCallback<void(ui::test::EventGenerator&)> invocation) {
  // Run the callback with a std::ref to this object's event generator since
  // it is stateful.
  WlcsEnvironment::Get().env.RunOnUiThreadBlocking(
      base::BindOnce(std::move(invocation), std::ref(*evg_)));
}

}  // namespace exo::wlcs
