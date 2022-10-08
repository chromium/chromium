// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_H_
#define COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_H_

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/reporting/health/health_module_delegate.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/status.h"

namespace reporting {

// The HealthModule class is used by other modules in the ERP to update and
// gather health related info. This class delegates the implementation logic to
// the HealthModuleDelegate and ensures that all calls to read and write data
// are done with mutual exclusion
class HealthModule : public base::RefCountedThreadSafe<HealthModule> {
 public:
  // Static class factory method.
  static scoped_refptr<HealthModule> Create(
      std::unique_ptr<HealthModuleDelegate> delegate);

  HealthModule(const HealthModule& other) = delete;
  HealthModule& operator=(const HealthModule& other) = delete;

  // Adds history record to local memory. Triggers a write to health files.
  void PostHealthRecord(HealthDataHistory history);

  // Gets health data and send to |cb|.
  void GetHealthData(base::OnceCallback<void(const ERPHealthData)> cb);

 protected:
  // Constructor can only be called by |Create| factory method.
  HealthModule(std::unique_ptr<HealthModuleDelegate> delegate,
               scoped_refptr<base::SequencedTaskRunner> task_runner);

  // HealthModuleDelegate controlling read/write logic.
  std::unique_ptr<HealthModuleDelegate> delegate_;

  virtual ~HealthModule();  // `virtual` is mandated by RefCounted.

 private:
  // Task Runner which tasks are posted to.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  friend base::RefCountedThreadSafe<HealthModule>;
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_H_
