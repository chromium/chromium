// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_STORAGE_APP_STORAGE_FILE_HANDLER_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_STORAGE_APP_STORAGE_FILE_HANDLER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/values.h"
#include "components/services/app_service/public/cpp/app.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace apps {

// AppStorageFileHandler is the backend used by AppStorage to reads and writes
// the app information from and to disk. AppStorageFileHandler is created on the
// main thread, but the real reading and writing work must be invoked on a
// background task runner `owning_task_runner`.
class COMPONENT_EXPORT(APP_UPDATE) AppStorageFileHandler
    : public base::RefCountedDeleteOnSequence<AppStorageFileHandler> {
 public:
  struct AppInfo {
    AppInfo();
    ~AppInfo();
    std::vector<AppPtr> apps;
    std::set<AppType> app_types;
  };

  // Creates a AppStorageFileHandler. This method is invoked on the main
  // thread, and does no IO. `base_path` is the path of the app storage file.
  explicit AppStorageFileHandler(const base::FilePath& base_path);

  AppStorageFileHandler(const AppStorageFileHandler&) = delete;
  AppStorageFileHandler& operator=(const AppStorageFileHandler&) = delete;

  base::SequencedTaskRunner* owning_task_runner() {
    return base::RefCountedDeleteOnSequence<
        AppStorageFileHandler>::owning_task_runner();
  }

  // Writes `apps` to the AppStorage file. This method must be invoked on a
  // background task runner `owning_task_runner`.
  void WriteToFile(std::vector<AppPtr> apps);

  // Reads the app info from the AppStorage file. This method must be invoked on
  // a background task runner `owning_task_runner`.
  std::unique_ptr<AppInfo> ReadFromFile();

  const base::FilePath& GetFilePath() { return file_path_; }

 private:
  friend class base::RefCountedDeleteOnSequence<AppStorageFileHandler>;
  friend class base::DeleteHelper<AppStorageFileHandler>;

  virtual ~AppStorageFileHandler();

  // Converts std::vector<AppPtr> to base::Value,
  // e.g.:
  // {
  //   "agimnkijcaahngcdmfeangaknmldooml":    // app_id
  //     {
  //         "type": 5,
  //         "name": "YouTube",
  //         ...
  //     },
  //   "cnbgggchhmkkdmeppjobngjoejnihlei":    // app_id
  //     {
  //         "type": 1,
  //         "name": "PlayStore",
  //         ...
  //     },
  //   ...
  // }
  base::Value ConvertAppsToValue(std::vector<AppPtr> apps);

  // Converts base::Value to std::vector<AppPtr>,
  std::unique_ptr<AppInfo> ConvertValueToApps(base::Value app_info_value);

  base::FilePath file_path_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_STORAGE_APP_STORAGE_FILE_HANDLER_H_
