// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_FULL_RESTORE_READ_HANDLER_H_
#define COMPONENTS_FULL_RESTORE_FULL_RESTORE_READ_HANDLER_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"

namespace base {
class FilePath;
}  // namespace base

namespace full_restore {

class FullRestoreFileHandler;
class RestoreData;

// FullRestoreSaveHandler is responsible for reading |RestoreData| from the full
// restore data file. RestoreHandler runs on the main thread and creates
// FullRestoreFileHandler (which runs on a background task runner) for the
// actual reading.
class COMPONENT_EXPORT(FULL_RESTORE) FullRestoreReadHandler {
 public:
  // The callback function to get the restore data when the reading operation is
  // done.
  using Callback = base::OnceCallback<void(std::unique_ptr<RestoreData>)>;

  static FullRestoreReadHandler* GetInstance();

  FullRestoreReadHandler();
  virtual ~FullRestoreReadHandler();

  FullRestoreReadHandler(const FullRestoreReadHandler&) = delete;
  FullRestoreReadHandler& operator=(const FullRestoreReadHandler&) = delete;

  // Reads the restore data from |profile_path| on a background task runner, and
  // calls |callback| when the reading operation is done.
  void ReadFromFile(const base::FilePath& profile_path, Callback callback);

  // Removes app launching and app windows for an app with the given |app_id|
  // from |profile_path_to_restore_data_| for |profile_path| .
  void RemoveApp(const base::FilePath& profile_path, const std::string& app_id);

 private:
  // Invoked when reading the restore data from |profile_path| is finished, and
  // calls |callback| to notify that the reading operation is done.
  void OnGetRestoreData(const base::FilePath& profile_path,
                        Callback callback,
                        std::unique_ptr<RestoreData>);

  // The restore data read from the full restore files.
  std::map<base::FilePath, std::unique_ptr<RestoreData>>
      profile_path_to_restore_data_;

  base::WeakPtrFactory<FullRestoreReadHandler> weak_factory_{this};
};

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_FULL_RESTORE_READ_HANDLER_H_
