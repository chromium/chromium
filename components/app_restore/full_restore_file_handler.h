// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_FULL_RESTORE_FILE_HANDLER_H_
#define COMPONENTS_APP_RESTORE_FULL_RESTORE_FILE_HANDLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/task/sequenced_task_runner_helpers.h"

class SequencedTaskRunner;

namespace app_restore {
class RestoreData;
}

namespace full_restore {

// FullRestoreFileHandler is the backend used by FullRestoreSaveHandler and
// RestoreHandler. It reads and writes RestoreData from and to disk.
// FullRestoreFileHandler is created on the main thread, and does no IO by
// the constructor. The real work is done by WriteToFile and ReadFromFile, which
// must be invoked on a background task runner |owning_task_runner|.
class COMPONENT_EXPORT(APP_RESTORE) FullRestoreFileHandler
    : public base::RefCountedDeleteOnSequence<FullRestoreFileHandler> {
 public:
  // Creates a FullRestoreFileHandler. This method is invoked on the main
  // thread, and does no IO. |path| is the path of the full restore file.
  FullRestoreFileHandler(const base::FilePath& path);

  FullRestoreFileHandler(const FullRestoreFileHandler&) = delete;
  FullRestoreFileHandler& operator=(const FullRestoreFileHandler&) = delete;

  base::SequencedTaskRunner* owning_task_runner() {
    return base::RefCountedDeleteOnSequence<
        FullRestoreFileHandler>::owning_task_runner();
  }

  // Writes |restore_data| to the full restore file. This method must be invoked
  // on a background task runner |owning_task_runner|.
  void WriteToFile(std::unique_ptr<app_restore::RestoreData> restore_data);

  // Reads |restore_data| to the full restore file. This method must be invoked
  // on a background task runner |owning_task_runner|.
  std::unique_ptr<app_restore::RestoreData> ReadFromFile();

 private:
  friend class base::RefCountedDeleteOnSequence<FullRestoreFileHandler>;
  friend class base::DeleteHelper<FullRestoreFileHandler>;

  virtual ~FullRestoreFileHandler();

  // Performs blocking I/O. Called on a background task runner
  // |owning_task_runner|.
  void WriteDataBlocking(const std::string& full_restore_data);

  // Performs blocking I/O. Called on a background task runner.
  // |owning_task_runner|. Returns true on success and false on error. The
  // reading result is written to |full_restore_data|.
  bool ReadDataBlocking(std::string& full_restore_data);

  base::FilePath file_path_;
};

}  // namespace full_restore

#endif  // COMPONENTS_APP_RESTORE_FULL_RESTORE_FILE_HANDLER_H_
