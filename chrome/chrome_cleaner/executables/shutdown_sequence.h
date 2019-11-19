// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_EXECUTABLES_SHUTDOWN_SEQUENCE_H_
#define CHROME_CHROME_CLEANER_EXECUTABLES_SHUTDOWN_SEQUENCE_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/chrome_cleaner/engines/broker/engine_client.h"
#include "chrome/chrome_cleaner/engines/controllers/engine_facade_interface.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"

namespace chrome_cleaner {

// Helper to ensure correct order of destruction of mojo-related objects during
// the shutdown sequence. For it to work correctly, there must not be any other
// references to the object's fields.
struct ShutdownSequence {
  ShutdownSequence();
  ShutdownSequence(ShutdownSequence&& other);
  ~ShutdownSequence();

  scoped_refptr<EngineClient> engine_client;
  scoped_refptr<MojoTaskRunner> mojo_task_runner;
  std::unique_ptr<EngineFacadeInterface> engine_facade;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_EXECUTABLES_SHUTDOWN_SEQUENCE_H_
