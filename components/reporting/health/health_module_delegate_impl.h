// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_DELEGATE_IMPL_H_
#define COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_DELEGATE_IMPL_H_

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/reporting/health/health_module_delegate.h"
#include "components/reporting/health/health_module_files.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// The HealthModuleDelegateImpl is a prod implementation of
// HealthModule::Delegate interface. Keeps track of the local data of ERP Health
// and owns the HealthModuleFiles class which is written to and read from this
// class.
class HealthModuleDelegateImpl : public HealthModuleDelegate {
 public:
  explicit HealthModuleDelegateImpl(
      const base::FilePath& directory,
      size_t max_history_storage = kNaxHistoryStorage,
      std::string_view file_base_name = kHistoryFileBasename);
  HealthModuleDelegateImpl(const HealthModuleDelegateImpl& other) = delete;
  HealthModuleDelegateImpl& operator=(const HealthModuleDelegateImpl& other) =
      delete;
  ~HealthModuleDelegateImpl() override;

 private:
  static constexpr char kHistoryFileBasename[] = "health_info_";
  // Default max history size 100 KiB.
  static constexpr size_t kNaxHistoryStorage = 100 * 1024u;

  // Initializes files_. Returns error on failure.
  Status DoInit() override;

  // Gets health data from memory, sends to |cb|.
  void DoGetERPHealthData(HealthCallback cb) const override;

  // Writes a history record to files_, keeps track of data in local
  // memory. Can be overridden for tests.
  void DoPostHealthRecord(HealthDataHistory history) override;

  SEQUENCE_CHECKER(sequence_checker_);

  // Local copy of the health data. This is read from storage on startup and
  // written to locally. Occasionally this data is written back into memory.
  ERPHealthData data_in_memory_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Root directory of ERP Health data files.
  const base::FilePath directory_;

  const std::string file_base_name_;

  // Max storage used by health module.
  // TODO(tylergarrett) control each history per policy.
  const size_t max_history_storage_;

  // local storaged used to track health records.
  size_t storage_used_ GUARDED_BY_CONTEXT(sequence_checker_) = 0u;

  std::unique_ptr<HealthModuleFiles> files_
      GUARDED_BY_CONTEXT(sequence_checker_);
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_DELEGATE_IMPL_H_
