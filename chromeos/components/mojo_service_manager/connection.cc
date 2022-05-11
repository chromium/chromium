// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mojo_service_manager/connection.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cstdlib>
#include <utility>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/posix/eintr_wrapper.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace chromeos::mojo_service_manager {
namespace {

// The socket path to connect to the service manager.
constexpr char kServiceManagerSocketPath[] = "/run/mojo/service_manager";
// The default mojo pipe name for bootstrap the mojo.
constexpr int kDefaultMojoInvitationPipeName = 0;

base::ScopedFD ConnectToServiceManagerUnixSocket() {
  base::ScopedFD sock{socket(AF_UNIX, SOCK_STREAM, 0)};
  if (!sock.is_valid()) {
    PLOG(ERROR) << "Failed to create socket.";
    return base::ScopedFD{};
  }

  struct sockaddr_un unix_addr {
    .sun_family = AF_UNIX,
  };
  static_assert(sizeof(kServiceManagerSocketPath) <=
                sizeof(unix_addr.sun_path));
  strncpy(unix_addr.sun_path, kServiceManagerSocketPath,
          sizeof(kServiceManagerSocketPath));

  int rc = HANDLE_EINTR(connect(sock.get(),
                                reinterpret_cast<const sockaddr*>(&unix_addr),
                                sizeof(unix_addr)));
  if (rc == -1 && errno != EISCONN) {
    PLOG(ERROR) << "Failed to connect to service manager unix socket.";
    return base::ScopedFD{};
  }
  return sock;
}

mojo::PendingRemote<mojom::ServiceManager> ConnectToMojoServiceManager() {
  base::ScopedFD sock = ConnectToServiceManagerUnixSocket();
  if (!sock.is_valid())
    return mojo::PendingRemote<mojom::ServiceManager>{};
  auto invitation = mojo::IncomingInvitation::Accept(
      mojo::PlatformChannelEndpoint(mojo::PlatformHandle(std::move(sock))));
  mojo::ScopedMessagePipeHandle pipe =
      invitation.ExtractMessagePipe(kDefaultMojoInvitationPipeName);
  return mojo::PendingRemote<mojom::ServiceManager>(std::move(pipe), 0u);
}

mojo::Remote<mojom::ServiceManager>& GetRemote() {
  static base::NoDestructor<mojo::Remote<mojom::ServiceManager>> instance;
  return *instance;
}

}  // namespace

bool BootstrapServiceManagerConnection() {
  CHECK(!GetRemote().is_bound()) << "remote_ has already bound.";
  mojo::PendingRemote<mojom::ServiceManager> remote =
      ConnectToMojoServiceManager();
  if (!remote.is_valid())
    return false;
  GetRemote().Bind(std::move(remote));
  GetRemote().reset_on_disconnect();
  return true;
}

bool IsServiceManagerConnected() {
  // Because reset_on_disconnect() is set, is_bound() will be the same as
  // is_connected().
  DCHECK_EQ(GetRemote().is_bound(), GetRemote().is_connected());
  return GetRemote().is_bound();
}

void ResetServiceManagerConnection() {
  GetRemote().reset();
}

mojom::ServiceManagerProxy* GetServiceManagerProxy() {
  return GetRemote().get();
}

void SetServiceManagerRemoteForTesting(  // IN-TEST
    mojo::PendingRemote<mojom::ServiceManager> remote) {
  CHECK(remote.is_valid());
  GetRemote().Bind(std::move(remote));
  GetRemote().reset_on_disconnect();
}

}  // namespace chromeos::mojo_service_manager
