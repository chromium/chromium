// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_H_
#define COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_H_

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/reporting/health/health_module_delegate.h"
#include "components/reporting/proto/synced/health.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/status.h"

namespace reporting {

// The HealthModule class is used by other modules in the ERP to update and
// gather health related info. This class delegates the implementation logic to
// the HealthModuleDelegate and ensures that all calls to read and write data
// are done with mutual exclusion
class HealthModule : public base::RefCountedThreadSafe<HealthModule> {
 public:
  // Instance of `Recorder` class provides an easy to use access for
  // the caller to compose a single history record, which is posted when
  // the instance is destructed.
  // The class is non-copyable but moveable, so its instance may be handed
  // over by std::move from one stage of the process to another, until it
  // is destructed at the end of the processing (posting the accumulated
  // health history).
  class Recorder {
   public:
    // Recorder constructor is associated with health module;
    // nullptr is used only when debugging not enabled.
    explicit Recorder(scoped_refptr<HealthModule> health_module = nullptr);
    Recorder(Recorder&& other);
    Recorder& operator=(Recorder&& other);
    ~Recorder();

    // Returns true if debuggung is active (health_module is present).
    // When the result is false, other actions are not doing anything.
    explicit operator bool() const noexcept;

    // Accessors that present history record to be set up.
    HealthDataHistory& operator*() noexcept;
    HealthDataHistory* operator->() noexcept;

   private:
    HealthDataHistory history_;
    scoped_refptr<HealthModule> health_module_;
  };

  // Static class factory method.
  static scoped_refptr<HealthModule> Create(
      std::unique_ptr<HealthModuleDelegate> delegate);

  HealthModule(const HealthModule& other) = delete;
  HealthModule& operator=(const HealthModule& other) = delete;

  // Adds history record to local memory. Triggers a write to health files.
  void PostHealthRecord(HealthDataHistory history);

  // Gets health data and send to |cb|.
  void GetHealthData(HealthCallback cb);

 protected:
  // Constructor can only be called by |Create| factory method.
  HealthModule(std::unique_ptr<HealthModuleDelegate> delegate,
               scoped_refptr<base::SequencedTaskRunner> task_runner);

  // HealthModuleDelegate controlling read/write logic.
  std::unique_ptr<HealthModuleDelegate> delegate_;

  virtual ~HealthModule();  // `virtual` is mandated by RefCounted.

 private:
  friend base::RefCountedThreadSafe<HealthModule>;

  // Task Runner which tasks are posted to.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_H_
