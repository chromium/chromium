// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_TARGET_TEST_ENGINE_DELEGATE_H_
#define CHROME_CHROME_CLEANER_ENGINES_TARGET_TEST_ENGINE_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/threading/thread.h"
#include "chrome/chrome_cleaner/engines/target/engine_delegate.h"

namespace chrome_cleaner {

// An EngineDelegate that detects only test UwS.
class TestEngineDelegate : public EngineDelegate {
 public:
  TestEngineDelegate();

  Engine::Name engine() const override;

  void Initialize(
      const base::FilePath& log_directory_path,
      scoped_refptr<EngineFileRequestsProxy> privileged_file_calls,
      mojom::EngineCommands::InitializeCallback done_callback) override;

  uint32_t StartScan(
      const std::vector<UwSId>& enabled_uws,
      const std::vector<UwS::TraceLocation>& enabled_trace_locations,
      bool include_details,
      scoped_refptr<EngineFileRequestsProxy> privileged_file_calls,
      scoped_refptr<EngineRequestsProxy> privileged_scan_calls,
      scoped_refptr<EngineScanResultsProxy> report_result_calls) override;

  uint32_t StartCleanup(
      const std::vector<UwSId>& enabled_uws,
      scoped_refptr<EngineFileRequestsProxy> privileged_file_calls,
      scoped_refptr<EngineRequestsProxy> privileged_scan_calls,
      scoped_refptr<CleanerEngineRequestsProxy> privileged_removal_calls,
      scoped_refptr<EngineCleanupResultsProxy> report_result_calls) override;

  uint32_t Finalize() override;

 private:
  ~TestEngineDelegate() override;

  // The methods of this interface are invoked on the Mojo IO thread. Each will
  // immediately hand off the work to this thread, since the IO thread doesn't
  // allow nested run loops.
  std::unique_ptr<base::Thread> work_thread_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_TARGET_TEST_ENGINE_DELEGATE_H_
