// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "components/reporting/health/health_module_delegate.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/file.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

#ifndef COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_H_
#define COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_H_

namespace reporting {

// The HealthModule class is used by other modules in the ERP to update and
// gather health related info. This class delegates the implementation logic to
// the HealthModuleDelegate and ensures that all calls to read and write data
// are done with mutual exclusion
class HealthModule : public base::RefCountedThreadSafe<HealthModule> {
 public:
  // Factory constructor for use in production
  static scoped_refptr<HealthModule> Create(const base::FilePath& directory);

  HealthModule(const HealthModule& other) = delete;
  HealthModule& operator=(const HealthModule& other) = delete;

  // Add history record to local memory. Triggers a write to health files.
  void PostHealthRecord(HealthDataHistory history);

  // Get health data and send to |cb|.
  void GetHealthData(base::OnceCallback<void(const ERPHealthData)> cb);

 protected:
  // Constructor can only be called by |Create| factory method.
  HealthModule(const base::FilePath& directory,
               scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Delegate controlling read/write logic.
  std::unique_ptr<HealthModuleDelegate> delegate_;

  virtual ~HealthModule();

 private:
  // Task Runner which tasks are posted to.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Default max history size. 100 KB.
  const uint32_t max_history_bytes_ = 100000;

  friend base::RefCountedThreadSafe<HealthModule>;
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_H_
