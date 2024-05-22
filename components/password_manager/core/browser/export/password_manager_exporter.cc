// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_manager_exporter.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/export/password_csv_writer.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

namespace password_manager {

namespace {

// Multiple exports should be queued in sequence. This helps avoid race
// conditions where there are multiple simultaneous exports to the same
// destination and one of them was cancelled and will delete the file. We use
// TaskPriority::USER_VISIBLE, because a busy UI is displayed while the
// passwords are being exported.
base::LazyThreadPoolSingleThreadTaskRunner g_task_runner =
    LAZY_THREAD_POOL_SINGLE_THREAD_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(), base::TaskPriority::USER_VISIBLE),
        base::SingleThreadTaskRunnerThreadMode::SHARED);

// A wrapper for |write_function|, which can be bound and keep a copy of its
// data on the closure.
bool DoWriteOnTaskRunner(
    PasswordManagerExporter::WriteCallback write_function,
    PasswordManagerExporter::SetPosixFilePermissionsCallback
        set_permissions_function,
    const base::FilePath& destination,
    const std::string& serialised) {
  if (!write_function.Run(destination, serialised)) {
    return false;
  }

  // Set file permissions. This is a no-op outside of Posix.
  set_permissions_function.Run(destination, 0600 /* -rw------- */);
  return true;
}

bool DefaultWriteFunction(const base::FilePath& file, std::string_view data) {
  return base::WriteFile(file, data);
}

bool DefaultDeleteFunction(const base::FilePath& file) {
  return base::DeleteFile(file);
}

}  // namespace

PasswordManagerExporter::PasswordManagerExporter(
    SavedPasswordsPresenter* presenter,
    ProgressCallback on_progress,
    base::OnceClosure completion_callback)
    : presenter_(presenter),
      on_progress_(std::move(on_progress)),
      last_progress_status_(ExportProgressStatus::kNotStarted),
      write_function_(base::BindRepeating(&DefaultWriteFunction)),
      delete_function_(base::BindRepeating(&DefaultDeleteFunction)),
      completion_callback_(std::move(completion_callback)),
#if BUILDFLAG(IS_POSIX)
      set_permissions_function_(
          base::BindRepeating(base::SetPosixFilePermissions)),
#else
      set_permissions_function_(
          base::BindRepeating([](const base::FilePath&, int) { return true; })),
#endif
      task_runner_(g_task_runner.Get()) {
}

PasswordManagerExporter::~PasswordManagerExporter() = default;

void PasswordManagerExporter::PreparePasswordsForExport() {
  DCHECK_EQ(GetProgressStatus(), ExportProgressStatus::kNotStarted);

  std::vector<CredentialUIEntry> credentials =
      presenter_->GetSavedCredentials();
  // Clear blocked credentials.
  std::erase_if(credentials, [](const auto& credential) {
    return credential.blocked_by_user;
  });

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PasswordCSVWriter::SerializePasswords, credentials),
      base::BindOnce(&PasswordManagerExporter::SetSerialisedPasswordList,
                     weak_factory_.GetWeakPtr(), credentials.size()));
}

void PasswordManagerExporter::SetDestination(
    const base::FilePath& destination) {
  DCHECK_EQ(GetProgressStatus(), ExportProgressStatus::kNotStarted);

  destination_ = destination;

  if (IsReadyForExport()) {
    Export();
  }

  OnProgress({.status = ExportProgressStatus::kInProgress});
}

void PasswordManagerExporter::SetSerialisedPasswordList(
    size_t count,
    const std::string& serialised) {
  serialised_password_list_ = serialised;
  password_count_ = count;
  if (IsReadyForExport()) {
    Export();
  }
}

void PasswordManagerExporter::Cancel() {
  // Tasks which had their pointers invalidated won't run.
  weak_factory_.InvalidateWeakPtrs();

  // If we are currently still serialising, Export() will see the cancellation
  // status and won't schedule writing.
  OnProgress({.status = ExportProgressStatus::kFailedCancelled});

  // If we are currently writing to the disk, we will have to cleanup the file
  // once writing stops.
  Cleanup();

  // Resets the unique pointer to the current object instance.
  std::move(completion_callback_).Run();
}

ExportProgressStatus PasswordManagerExporter::GetProgressStatus() {
  return last_progress_status_;
}

void PasswordManagerExporter::SetWriteForTesting(WriteCallback write_function) {
  write_function_ = std::move(write_function);
}

void PasswordManagerExporter::SetDeleteForTesting(
    DeleteCallback delete_callback) {
  delete_function_ = std::move(delete_callback);
}

void PasswordManagerExporter::SetSetPosixFilePermissionsForTesting(
    SetPosixFilePermissionsCallback set_permissions_callback) {
  set_permissions_function_ = std::move(set_permissions_callback);
}

bool PasswordManagerExporter::IsReadyForExport() {
  return !destination_.empty() && !serialised_password_list_.empty();
}

void PasswordManagerExporter::Export() {
  // If cancelling was requested while we were serialising the passwords, don't
  // write anything to the disk.
  if (GetProgressStatus() == ExportProgressStatus::kFailedCancelled) {
    serialised_password_list_.clear();
    return;
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(DoWriteOnTaskRunner, write_function_,
                     set_permissions_function_, destination_,
                     std::move(serialised_password_list_)),
      base::BindOnce(&PasswordManagerExporter::OnPasswordsExported,
                     weak_factory_.GetWeakPtr()));
}

void PasswordManagerExporter::OnPasswordsExported(bool success) {
  if (success) {
#if !BUILDFLAG(IS_WIN)
    std::string file_path = destination_.value();
#else
    std::string file_path = base::WideToUTF8(destination_.value());
#endif
    OnProgress(
        {.status = ExportProgressStatus::kSucceeded, .file_path = file_path});

  } else {
    OnProgress(
        {.status = ExportProgressStatus::kFailedWrite,
         .folder_name = destination_.DirName().BaseName().AsUTF8Unsafe()});
    // Don't leave partial password files, if we tell the user we couldn't write
    Cleanup();
  }

  // Resets the unique pointer to the current object instance.
  std::move(completion_callback_).Run();
}

void PasswordManagerExporter::OnProgress(const PasswordExportInfo& progress) {
  last_progress_status_ = progress.status;
  on_progress_.Run(progress);
}

void PasswordManagerExporter::Cleanup() {
  // The PasswordManagerExporter instance may be destroyed before the cleanup is
  // executed, e.g. because a new export was initiated. The cleanup should be
  // carried out regardless, so we only schedule tasks which own their
  // arguments.
  // TODO(crbug.com/41370350) When Chrome is overwriting an existing file,
  // cancel should restore the file rather than delete it.
  if (!destination_.empty()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(delete_function_), destination_));
  }
}

}  // namespace password_manager
