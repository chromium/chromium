// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_SCHEDULED_EXECUTOR_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_SCHEDULED_EXECUTOR_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/abseil-cpp/absl/time/time.h"
#include "third_party/nearby/src/internal/platform/implementation/scheduled_executor.h"

namespace base {
class SequencedTaskRunner;
}

namespace nearby::chrome {

// Concrete ScheduledExecutor implementation.
class ScheduledExecutor : public api::ScheduledExecutor {
 public:
  class Core;

  explicit ScheduledExecutor(
      scoped_refptr<base::SequencedTaskRunner> timer_task_runner);
  ~ScheduledExecutor() override;

  ScheduledExecutor(const ScheduledExecutor&) = delete;
  ScheduledExecutor& operator=(const ScheduledExecutor&) = delete;

  // api::ScheduledExecutor:
  std::shared_ptr<api::Cancelable> Schedule(Runnable&& runnable,
                                            absl::Duration duration) override;
  void Execute(Runnable&& runnable) override;
  void Shutdown() override;

 private:
  scoped_refptr<Core> core_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_SCHEDULED_EXECUTOR_H_
