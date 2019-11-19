// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_DELEGATE_H_
#define CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_DELEGATE_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/engines/target/cleaner_engine_requests_proxy.h"
#include "chrome/chrome_cleaner/engines/target/engine_cleanup_results_proxy.h"
#include "chrome/chrome_cleaner/engines/target/engine_file_requests_proxy.h"
#include "chrome/chrome_cleaner/engines/target/engine_requests_proxy.h"
#include "chrome/chrome_cleaner/engines/target/engine_scan_results_proxy.h"
#include "chrome/chrome_cleaner/mojom/engine_sandbox.mojom.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"

namespace chrome_cleaner {

// Provides an interface to communicate with an arbitrary engine.
// Subclasses define engine-specific implementation for initialization,
// scan and cleanup dispatch, and finalization.
class EngineDelegate : public base::RefCountedThreadSafe<EngineDelegate> {
 public:
  EngineDelegate();

  virtual Engine::Name engine() const = 0;

  // Implemented by subclasses to invoke the engine initialization. If not
  // empty, |log_directory_path| corresponds to the directory where logs can
  // be saved. Implementations must invoke |callback| once initialization
  // is done and invocation can happen on any thread. The caller will wait
  // until |callback| is invoked.
  virtual void Initialize(
      const base::FilePath& log_directory_path,
      scoped_refptr<EngineFileRequestsProxy> privileged_file_calls,
      mojom::EngineCommands::InitializeCallback done_callback) = 0;

  virtual uint32_t StartScan(
      const std::vector<UwSId>& enabled_uws,
      const std::vector<UwS::TraceLocation>& enabled_trace_locations,
      bool include_details,
      scoped_refptr<EngineFileRequestsProxy> privileged_file_calls,
      scoped_refptr<EngineRequestsProxy> privileged_scan_calls,
      scoped_refptr<EngineScanResultsProxy> report_result_calls) = 0;

  virtual uint32_t StartCleanup(
      const std::vector<UwSId>& enabled_uws,
      scoped_refptr<EngineFileRequestsProxy> privileged_file_calls,
      scoped_refptr<EngineRequestsProxy> privileged_scan_calls,
      scoped_refptr<CleanerEngineRequestsProxy> privileged_removal_calls,
      scoped_refptr<EngineCleanupResultsProxy> report_result_calls) = 0;

  virtual uint32_t Finalize() = 0;

 protected:
  virtual ~EngineDelegate();

 private:
  friend class base::RefCountedThreadSafe<EngineDelegate>;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_DELEGATE_H_
