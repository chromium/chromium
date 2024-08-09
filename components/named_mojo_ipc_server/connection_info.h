// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_MOJO_IPC_SERVER_CONNECTION_INFO_H_
#define COMPONENTS_NAMED_MOJO_IPC_SERVER_CONNECTION_INFO_H_

#include "base/process/process_handle.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_handle.h"
#elif BUILDFLAG(IS_MAC)
#include <bsm/libbsm.h>
#elif BUILDFLAG(IS_LINUX)
#include <sys/socket.h>
#endif

namespace named_mojo_ipc_server {

// ConnectionInfo encapsulates information useful for verifying a potential
// endpoint.
struct ConnectionInfo {
  ConnectionInfo();
  ~ConnectionInfo();
  ConnectionInfo(const ConnectionInfo&) = delete;
  ConnectionInfo& operator=(const ConnectionInfo&) = delete;

  base::ProcessId pid{};
#if BUILDFLAG(IS_MAC)
  audit_token_t audit_token{};
#elif BUILDFLAG(IS_LINUX)
  ucred credentials{};
#endif
};

}  // namespace named_mojo_ipc_server

#endif  // COMPONENTS_NAMED_MOJO_IPC_SERVER_CONNECTION_INFO_H_
