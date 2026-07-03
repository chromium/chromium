// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_CHANNEL_LINUX_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_CHANNEL_LINUX_H_

#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/channel_posix.h"

namespace mojo_legacy::core {

// ChannelLinux is a specialization of ChannelPosix. By default on Linux, CrOS,
// and Android every channel will be of type ChannelLinux. The shared memory
// channel upgrade mechanism formerly implemented by this class is not
// supported; incoming upgrade offers are rejected by ChannelPosix.
class MOJO_LEGACY_SYSTEM_IMPL_EXPORT ChannelLinux : public ChannelPosix {
 public:
  ChannelLinux(Delegate* delegate,
               ConnectionParams connection_params,
               HandlePolicy handle_policy,
               scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  ChannelLinux(const ChannelLinux&) = delete;
  ChannelLinux& operator=(const ChannelLinux&) = delete;

 private:
  ~ChannelLinux() override;
};

}  // namespace mojo_legacy::core

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_CHANNEL_LINUX_H_
