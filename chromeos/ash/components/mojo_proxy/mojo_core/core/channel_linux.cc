// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mojo_proxy/mojo_core/core/channel_linux.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"

namespace mojo_legacy::core {

ChannelLinux::ChannelLinux(
    Delegate* delegate,
    ConnectionParams connection_params,
    HandlePolicy handle_policy,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : ChannelPosix(delegate,
                   std::move(connection_params),
                   handle_policy,
                   io_task_runner) {}

ChannelLinux::~ChannelLinux() = default;

}  // namespace mojo_legacy::core
