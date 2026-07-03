// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_BROKER_HOST_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_BROKER_HOST_H_

#include <stdint.h>

#include <string_view>
#include <vector>

#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/channel.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/connection_params.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/embedder/process_error_callback.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/platform_handle_in_transit.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/platform/platform_handle.h"

namespace mojo_legacy {
namespace core {

// The BrokerHost is a channel to a broker client process, servicing synchronous
// IPCs issued by the client.
class BrokerHost : public Channel::Delegate,
                   public base::CurrentThread::DestructionObserver {
 public:
  BrokerHost(base::Process client_process,
             ConnectionParams connection_params,
             const ProcessErrorCallback& process_error_callback);

  BrokerHost(const BrokerHost&) = delete;
  BrokerHost& operator=(const BrokerHost&) = delete;

  // Send |handle| to the client, to be used to establish a NodeChannel to us.
  bool SendChannel(PlatformHandle handle);

#if BUILDFLAG(IS_WIN)
  // Sends a named channel to the client. Like above, but for named pipes.
  void SendNamedChannel(std::wstring_view pipe_name);
#endif

 private:
  ~BrokerHost() override;

  bool PrepareHandlesForClient(std::vector<PlatformHandleInTransit>* handles);

  // Channel::Delegate:
  void OnChannelMessage(const void* payload,
                        size_t payload_size,
                        std::vector<PlatformHandle> handles) override;
  void OnChannelError(Channel::Error error) override;

  // base::CurrentThread::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override;

  void OnBufferRequest(uint32_t num_bytes);

  const ProcessErrorCallback process_error_callback_;

#if BUILDFLAG(IS_WIN)
  base::Process client_process_;
#endif

  scoped_refptr<Channel> channel_;
};

}  // namespace core
}  // namespace mojo_legacy

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_BROKER_HOST_H_
