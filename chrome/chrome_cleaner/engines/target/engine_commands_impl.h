// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_COMMANDS_IMPL_H_
#define CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_COMMANDS_IMPL_H_

#include <stdint.h>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "chrome/chrome_cleaner/engines/target/engine_delegate.h"
#include "chrome/chrome_cleaner/mojom/cleaner_engine_requests.mojom.h"
#include "chrome/chrome_cleaner/mojom/engine_requests.mojom.h"
#include "chrome/chrome_cleaner/mojom/engine_sandbox.mojom.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chrome_cleaner {

class EngineCommandsImpl : public mojom::EngineCommands {
 public:
  EngineCommandsImpl(scoped_refptr<EngineDelegate> engine_delegate,
                     mojo::PendingReceiver<mojom::EngineCommands> receiver,
                     scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     base::OnceClosure error_handler);
  ~EngineCommandsImpl() override;

  // mojom::EngineCommands

  void Initialize(
      mojo::PendingAssociatedRemote<mojom::EngineFileRequests> file_requests,
      const base::FilePath& log_directory,
      InitializeCallback callback) override;
  void StartScan(
      const std::vector<UwSId>& enabled_uws,
      const std::vector<UwS::TraceLocation>& enabled_trace_locations,
      bool include_details,
      mojo::PendingAssociatedRemote<mojom::EngineFileRequests> file_requests,
      mojo::PendingAssociatedRemote<mojom::EngineRequests>
          sandboxed_engine_requests,
      mojo::PendingAssociatedRemote<mojom::EngineScanResults> scan_results,
      StartScanCallback callback) override;
  void StartCleanup(
      const std::vector<UwSId>& enabled_uws,
      mojo::PendingAssociatedRemote<mojom::EngineFileRequests> file_requests,
      mojo::PendingAssociatedRemote<mojom::EngineRequests>
          sandboxed_engine_requests,
      mojo::PendingAssociatedRemote<mojom::CleanerEngineRequests>
          sandboxed_cleaner_engine_requests,
      mojo::PendingAssociatedRemote<mojom::EngineCleanupResults>
          cleanup_results,
      StartCleanupCallback callback) override;
  void Finalize(FinalizeCallback callback) override;

 private:
  // Invokes |callback(result_code)| on the sequence of |task_runner_|.
  void PostInitializeCallback(
      mojom::EngineCommands::InitializeCallback callback,
      uint32_t result_code);

  scoped_refptr<EngineDelegate> engine_delegate_;
  mojo::Receiver<mojom::EngineCommands> receiver_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_COMMANDS_IMPL_H_
