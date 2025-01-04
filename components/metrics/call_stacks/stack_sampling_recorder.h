// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACKS_STACK_SAMPLING_RECORDER_H_
#define COMPONENTS_METRICS_CALL_STACKS_STACK_SAMPLING_RECORDER_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "components/metrics/call_stacks/call_stack_profile_metrics_provider.h"

namespace metrics {

// Instructs Chrome to record /tmp/stack-sampling-data with details of which
// threads and processes are being successfully stack-sampled.
inline constexpr char kRecordStackSamplingDataSwitch[] =
    "record-stack-sampling-data";

// Regularly writes a file (default: /tmp/stack-sampling-data) listing which
// threads and processes have been successfully stack sampled.
//
// Used by a few ChromeOS tast tests. Not created during normal operations.
// Creation is controlled by kRecordStackSamplingDataSwitch.
class StackSamplingRecorder
    : public base::RefCountedThreadSafe<StackSamplingRecorder> {
 public:
  StackSamplingRecorder();
  StackSamplingRecorder(const StackSamplingRecorder&) = delete;
  StackSamplingRecorder& operator=(const StackSamplingRecorder&) = delete;

  void Start();

 private:
  friend class base::RefCountedThreadSafe<StackSamplingRecorder>;
  // Friends to access some test-only functions
  friend class TestingStackSamplingRecorder;

  // Constructor for testing.
  explicit StackSamplingRecorder(base::FilePath file_path);
  virtual ~StackSamplingRecorder();

  // Calls
  // metrics::CallStackProfileMetricsProvider::GetSuccessfullyCollectedCounts()
  // Virtual so we can override in tests.
  virtual metrics::CallStackProfileMetricsProvider::ProcessThreadCount
  GetSuccessfullyCollectedCounts() const;

  // File writing callback.
  void WriteFile();

  // Helper for WriteFile(). A separate function so that we can return early on
  // error, but always post a retry task inside WriteFile().
  void WriteFileHelper();

  // The path that the stack sampling data is written to.
  const base::FilePath file_path_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACKS_STACK_SAMPLING_RECORDER_H_
