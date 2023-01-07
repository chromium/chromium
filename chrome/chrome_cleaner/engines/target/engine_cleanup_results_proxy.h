// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_CLEANUP_RESULTS_PROXY_H_
#define CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_CLEANUP_RESULTS_PROXY_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/chrome_cleaner/mojom/engine_sandbox.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace chrome_cleaner {

// Accessors to send the cleanup results over the Mojo connection.
class EngineCleanupResultsProxy
    : public base::RefCountedThreadSafe<EngineCleanupResultsProxy> {
 public:
  EngineCleanupResultsProxy(
      mojo::PendingAssociatedRemote<mojom::EngineCleanupResults>
          cleanup_results,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

  void UnbindCleanupResults();

  // Sends a cleanup done signal to the broken process. Will be called on an
  // arbitrary thread from the sandboxed engine.
  void CleanupDone(uint32_t result);

 protected:
  // Tests can subclass this create a proxy that's not bound to anything.
  EngineCleanupResultsProxy();

 private:
  friend class base::RefCountedThreadSafe<EngineCleanupResultsProxy>;
  ~EngineCleanupResultsProxy();

  // Invokes cleanup_results_->Done from the IPC thread.
  void OnDone(uint32_t result);

  // An EngineCleanupResults that will send the results over the Mojo
  // connection.
  mojo::AssociatedRemote<mojom::EngineCleanupResults> cleanup_results_;

  // A task runner for the IPC thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_CLEANUP_RESULTS_PROXY_H_
