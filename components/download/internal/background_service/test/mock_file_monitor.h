// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_MOCK_FILE_MONITOR_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_MOCK_FILE_MONITOR_H_

#include "components/download/internal/background_service/file_monitor.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace download {

class MockFileMonitor : public FileMonitor {
 public:
  MockFileMonitor();
  ~MockFileMonitor() override;

  void TriggerInit(bool success);
  void TriggerHardRecover(bool success);

  void Initialize(FileMonitor::InitCallback callback) override;
  MOCK_METHOD(void,
              DeleteUnknownFiles,
              (const Model::EntryList&,
               const std::vector<DriverEntry>&,
               base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              CleanupFilesForCompletedEntries,
              (const Model::EntryList&, base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              DeleteFiles,
              (const std::set<base::FilePath>&, stats::FileCleanupReason),
              (override));
  void HardRecover(FileMonitor::InitCallback) override;

 private:
  FileMonitor::InitCallback init_callback_;
  FileMonitor::InitCallback recover_callback_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_MOCK_FILE_MONITOR_H_
