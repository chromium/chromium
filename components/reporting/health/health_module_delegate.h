// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/reporting/health/health_module_files.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

#ifndef COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_DELEGATE_H_
#define COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_DELEGATE_H_

namespace reporting {

using HealthCallback = base::OnceCallback<void(const ERPHealthData)>;

// The HealthModuleDelegate is a class that servers as an interface between
// the Health Module and the underlying data that needs to be stored. The
// delegate keeps track of the local data of ERP Health and owns The
// HealthModuleFiles class which is written to and read from this class.
class HealthModuleDelegate {
 public:
  HealthModuleDelegate(const base::FilePath& directory,
                       base::StringPiece file_base_name,
                       size_t max_history_storage);
  ~HealthModuleDelegate();

  HealthModuleDelegate(const HealthModuleDelegate& other) = delete;
  HealthModuleDelegate& operator=(const HealthModuleDelegate& other) = delete;

  // Gets a copy of health data and runs a callback with it.
  void GetERPHealthData(HealthCallback cb) const;

  // Writes a history record to files_, keeps track of data in local memory.
  void PostHealthRecord(HealthDataHistory history);

  // Initialization logic for files_
  void Init();

  bool IsInitialized() const;

  base::WeakPtr<HealthModuleDelegate> GetWeakPtr() const;

 private:
  // Local copy of the health data. This is read from storage on startup and
  // written to locally. Occasionally this data is written back into memory.
  ERPHealthData data_in_memory_;

  // Root directory of ERP Health data files.
  const base::FilePath directory_;

  const std::string file_base_name_;

  // Max storage used by health module.
  // TODO(tylergarrett) control each history per policy.
  const size_t max_history_storage_;

  // local storaged used to track health records.
  size_t storage_used_ = 0;

  bool initialized_ = false;

  std::unique_ptr<HealthModuleFiles> files_;

  base::WeakPtrFactory<HealthModuleDelegate> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_DELEGATE_H_
