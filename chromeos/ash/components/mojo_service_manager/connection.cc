// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mojo_service_manager/connection.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cstdlib>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace ash::mojo_service_manager {

namespace mojom = chromeos::mojo_service_manager::mojom;

namespace {

// The socket path to connect to the service manager.
constexpr char kServiceManagerSocketPath[] = "/run/mojo/service_manager.sock";
// The default mojo pipe name for bootstrap the mojo.
constexpr int kDefaultMojoInvitationPipeName = 0;

// The connection may fail if the socket is not exist or the permission is not
// set to the right permission. This could happen if the ChromeOS mojo service
// manager is starting. We may need to wait for a while and retry.
//
// TODO(b/234318452): Clean up this retry logic after we collect enough UMA
// data.
//
// The retry interval of connecting to the service manager. It is expected that
// normally the first retry should be able to perform the bootstrap on all
// devices.
constexpr base::TimeDelta kRetryInterval = base::Milliseconds(1);
// The retry timeout of connecting to the service manager.
constexpr base::TimeDelta kRetryTimeout = base::Seconds(5);

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

// Sends the histogram to record the retry times of bootstrap.
void SendBootsrapRetryTimesHistogram(int retry_times) {
  // Note that sample will be 0 if didn't retry, and it will be in the underflow
  // bucket.
  base::UmaHistogramCustomCounts("Ash.MojoServiceManager.BootstrapRetryTimes",
                                 retry_times, 1, 5000, 50);
}

// Sends the histogram to record whether the connection is lost during ash
// running.
void SendIsConnectionLostHistogram(bool is_connection_lost) {
  base::UmaHistogramBoolean("Ash.MojoServiceManager.IsConnectionLost",
                            is_connection_lost);
}

void OnDisconnect(uint32_t reason, const std::string& message) {
  SendIsConnectionLostHistogram(true);
  LOG(FATAL) << "Disconnecting from ChromeOS mojo service manager is "
                "unexpected. Reason: "
             << reason << ", message: " << message;
}

}  // namespace

bool BootstrapServiceManagerConnection() {
  CHECK(!GetRemote().is_bound()) << "remote_ has already bound.";

  // We block and sleep here because we assume that it doesn't need to retry at
  // all.
  int retry_times = 0;
  for (base::ElapsedTimer timer; timer.Elapsed() < kRetryTimeout;
       base::PlatformThread::Sleep(kRetryInterval), ++retry_times) {
    mojo::PendingRemote<mojom::ServiceManager> remote =
        ConnectToMojoServiceManager();
    if (!remote.is_valid())
      continue;
    SendBootsrapRetryTimesHistogram(retry_times);
    GetRemote().Bind(std::move(remote));
    GetRemote().set_disconnect_with_reason_handler(
        base::BindOnce(&OnDisconnect));
    return true;
  }
  SendBootsrapRetryTimesHistogram(retry_times);
  return false;
}

bool IsServiceManagerBound() {
  return GetRemote().is_bound();
}

void ResetServiceManagerConnection() {
  SendIsConnectionLostHistogram(false);
  GetRemote().reset();
}

mojom::ServiceManagerProxy* GetServiceManagerProxy() {
  return GetRemote().get();
}

void SetServiceManagerRemoteForTesting(  // IN-TEST
    mojo::PendingRemote<mojom::ServiceManager> remote) {
  CHECK(remote.is_valid());
  GetRemote().Bind(std::move(remote));
  GetRemote().set_disconnect_with_reason_handler(base::BindOnce(&OnDisconnect));
}

mojo::PendingReceiver<chromeos::mojo_service_manager::mojom::ServiceManager>
BootstrapServiceManagerInUtilityProcess() {
  return GetRemote().BindNewPipeAndPassReceiver();
}

}  // namespace ash::mojo_service_manager
