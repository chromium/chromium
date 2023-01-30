// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_DELEGATE_H_
#define COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/status.h"

namespace reporting {

using HealthCallback = base::OnceCallback<void(ERPHealthData)>;

// Interface between the Health Module and the underlying data that needs to
// be stored. Fully implemented for production, overridden for testing.
class HealthModuleDelegate {
 public:
  HealthModuleDelegate();
  HealthModuleDelegate(const HealthModuleDelegate& other) = delete;
  HealthModuleDelegate& operator=(const HealthModuleDelegate& other) = delete;
  virtual ~HealthModuleDelegate();

  // Initialization logic.
  void Init();

  // Gets a copy of health data and runs a callback with it.
  void GetERPHealthData(HealthCallback cb) const;

  // Writes a history record.
  void PostHealthRecord(HealthDataHistory history);

  bool IsInitialized() const;

  base::WeakPtr<HealthModuleDelegate> GetWeakPtr();

 protected:
  // Checker should be used by subclass too.
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // Implementation for prod, overrides for testing.
  virtual Status DoInit() = 0;
  virtual void DoGetERPHealthData(HealthCallback cb) const = 0;
  virtual void DoPostHealthRecord(HealthDataHistory history) = 0;

  bool initialized_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  base::WeakPtrFactory<HealthModuleDelegate> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_DELEGATE_H_
