// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_SANDBOX_LINUX_NACL_BPF_SANDBOX_LINUX_H_
#define COMPONENTS_NACL_LOADER_SANDBOX_LINUX_NACL_BPF_SANDBOX_LINUX_H_

#include "base/files/scoped_file.h"

namespace nacl {

bool InitializeBPFSandbox(base::ScopedFD proc_fd);

}  // namespace nacl

#endif  // COMPONENTS_NACL_LOADER_SANDBOX_LINUX_NACL_BPF_SANDBOX_LINUX_H_
