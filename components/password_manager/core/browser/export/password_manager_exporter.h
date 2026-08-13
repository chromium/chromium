// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_PASSWORD_MANAGER_EXPORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_PASSWORD_MANAGER_EXPORTER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/export/export_progress_status.h"

namespace password_manager {

class SavedPasswordsPresenter;

// Information about passwort export in progress.
struct PasswordExportInfo {
  friend bool operator==(const PasswordExportInfo&,
                         const PasswordExportInfo&) = default;

  ExportProgressStatus status;
  // The full path to the file with exported passwords.
  std::string file_path;
  // The name of the folder containing the exported file.
  std::string folder_name;
};

// Controls the exporting of passwords. One instance per export flow.
// PasswordManagerExporter will perform the export asynchronously as soon as all
// the required info is available (password list and destination), unless
// canceled.
class PasswordManagerExporter {
 public:
  using ProgressCallback =
      base::RepeatingCallback<void(const PasswordExportInfo&)>;
  using WriteCallback =
      base::RepeatingCallback<bool(const base::FilePath&, std::string_view)>;
  using DeleteCallback = base::RepeatingCallback<bool(const base::FilePath&)>;
  using SetPosixFilePermissionsCallback =
      base::RepeatingCallback<bool(const base::FilePath&, int)>;

  explicit PasswordManagerExporter(SavedPasswordsPresenter* presenter,
                                   ProgressCallback on_progress,
                                   base::OnceClosure completion_callback);

  PasswordManagerExporter(const PasswordManagerExporter&) = delete;
  PasswordManagerExporter& operator=(const PasswordManagerExporter&) = delete;

  virtual ~PasswordManagerExporter();

  // Pre-load the passwords from the password store.
  virtual void PreparePasswordsForExport();

  // Set the destination, where the passwords will be written when they are
  // ready. This is expected to be called after PreparePasswordsForExport().
  virtual void SetDestination(const base::FilePath& destination);

  // Cancel any pending exporting tasks and clear the file, if it was written
  // to the disk.
  virtual void Cancel();

  // Returns the most recent ExportProgressStatus value, as would have been
  // seen on the callback provided to the constructor.
  virtual ExportProgressStatus GetProgressStatus();

  // Replace the function which writes to the filesystem with a custom action.
  void SetWriteForTesting(WriteCallback write_callback);

  // Replace the function which writes to the filesystem with a custom action.
  void SetDeleteForTesting(DeleteCallback delete_callback);

  // Replace the function which sets file permissions on Posix with a custom
  // action.
  void SetSetPosixFilePermissionsForTesting(
      SetPosixFilePermissionsCallback set_permissions_callback);

 private:
  // Caches the serialised password list. It proceeds to export, if all the
  // parts are ready. |count| is the number of passwords which were serialised.
  // |serialised| is the serialised list of passwords.
  void SetSerialisedPasswordList(size_t count, const std::string& serialised);

  // Returns true if all the necessary data is available.
  bool IsReadyForExport();

  // Performs the export. It should not be called before the data is available.
  // At the end, it clears cached fields.
  void Export();

  // Callback after the passwords have been serialised. It reports the result to
  // the UI and to metrics. |destination| is the folder we wrote to. |count| is
  // the number of passwords exported. |success| is whether they were actually
  // written.
  void OnPasswordsExported(bool success);

  // Wrapper for the |on_progress_| callback, which caches |status|, so that
  // it can be provided by GetProgressStatus.
  void OnProgress(const PasswordExportInfo& progress);

  // Export failed or was cancelled. Restore the state of the file system by
  // removing any partial or unwanted files from the filesystem.
  void Cleanup();

  // The source of the password list which will be exported.
  const raw_ptr<SavedPasswordsPresenter> presenter_;

  // Callback to the UI.
  ProgressCallback on_progress_;

  // The most recent progress status update, as was seen on |on_progress_|.
  ExportProgressStatus last_progress_status_;

  // The password list that was read from the store and serialised.
  std::string serialised_password_list_;
  // The number of passwords that were serialised. Useful for metrics.
  size_t password_count_;

  // The destination which was provided and where the password list will be
  // sent. It will be cleared once exporting is complete.
  base::FilePath destination_;

  // The function which does the actual writing. It should wrap
  // base::WriteFile, unless it's changed for testing purposes.
  WriteCallback write_function_;

  // The function which does the actual deleting of a file. It should wrap
  // base::DeleteFile, unless it's changed for testing purposes.
  DeleteCallback delete_function_;

  // Resets the unique pointer to the current object instance.
  base::OnceClosure completion_callback_;

  // Set the file permissions on a file. It should wrap
  // base::SetPosixFilePermissions, unless this is not Posix or it's changed for
  // testing purposes.
  SetPosixFilePermissionsCallback set_permissions_function_;

  // |task_runner_| is used for time-consuming tasks during exporting. The tasks
  // will dereference a WeakPtr to |*this|, which means they all need to run on
  // the same sequence.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<PasswordManagerExporter> weak_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_PASSWORD_MANAGER_EXPORTER_H_
