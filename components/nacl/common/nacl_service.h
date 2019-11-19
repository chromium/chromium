// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_COMMON_NACL_SERVICE_H_
#define COMPONENTS_NACL_COMMON_NACL_SERVICE_H_

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/system/message_pipe.h"

// Helper which establishes and holds open an IPC connection to the parent
// process. This should exist as long as IPC needs to be possible.
class NaClService {
 public:
  explicit NaClService(
      scoped_refptr<base::SequencedTaskRunner> ipc_task_runner);
  ~NaClService();

  // Returns a message pipe to use for the client endpoint of a legacy IPC
  // Channel in this process. Must only be called once.
  mojo::ScopedMessagePipeHandle TakeChannelPipe();

 private:
  const mojo::core::ScopedIPCSupport ipc_support_;
};

#endif  // COMPONENTS_NACL_COMMON_NACL_SERVICE_H_
