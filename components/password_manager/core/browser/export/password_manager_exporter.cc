// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_manager_exporter.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/export/password_csv_writer.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"

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
  if (!write_function.Run(destination, serialised))
    return false;

  // Set file permissions. This is a no-op outside of Posix.
  set_permissions_function.Run(destination, 0600 /* -rw------- */);
  return true;
}

bool DefaultWriteFunction(const base::FilePath& file, base::StringPiece data) {
  return base::WriteFile(file, data);
}

bool DefaultDeleteFunction(const base::FilePath& file) {
  return base::DeleteFile(file);
}

std::vector<std::unique_ptr<PasswordForm>> DeduplicatePasswordsAcrossStores(
    std::vector<std::unique_ptr<PasswordForm>> passwords) {
  auto get_sort_key = [](const auto& password) {
    return CreateSortKey(*password, IgnoreStore(true));
  };
  auto cmp = [&](const auto& lhs, const auto& rhs) {
    return get_sort_key(lhs) < get_sort_key(rhs);
  };
  base::flat_set<std::unique_ptr<PasswordForm>, decltype(cmp)> unique_passwords(
      std::move(passwords), cmp);
  return std::move(unique_passwords).extract();
}

}  // namespace

PasswordManagerExporter::PasswordManagerExporter(
    CredentialProviderInterface* credential_provider_interface,
    ProgressCallback on_progress)
    : credential_provider_interface_(credential_provider_interface),
      on_progress_(std::move(on_progress)),
      last_progress_status_(ExportProgressStatus::NOT_STARTED),
      write_function_(base::BindRepeating(&DefaultWriteFunction)),
      delete_function_(base::BindRepeating(&DefaultDeleteFunction)),
#if defined(OS_POSIX)
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
  DCHECK_EQ(GetProgressStatus(), ExportProgressStatus::NOT_STARTED);

  std::vector<std::unique_ptr<PasswordForm>> password_list =
      credential_provider_interface_->GetAllPasswords();

  // Deduplicate passwords that are present in multiple stores, so the output
  // file doesn't contain repeated data.
  std::vector<std::unique_ptr<PasswordForm>> deduplicated_password_list =
      DeduplicatePasswordsAcrossStores(std::move(password_list));

  size_t deduplicated_password_list_size = deduplicated_password_list.size();
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&PasswordCSVWriter::SerializePasswords,
                     std::move(deduplicated_password_list)),
      base::BindOnce(&PasswordManagerExporter::SetSerialisedPasswordList,
                     weak_factory_.GetWeakPtr(),
                     deduplicated_password_list_size));
}

void PasswordManagerExporter::SetDestination(
    const base::FilePath& destination) {
  DCHECK_EQ(GetProgressStatus(), ExportProgressStatus::NOT_STARTED);

  destination_ = destination;

  if (IsReadyForExport())
    Export();

  OnProgress(ExportProgressStatus::IN_PROGRESS, std::string());
}

void PasswordManagerExporter::SetSerialisedPasswordList(
    size_t count,
    const std::string& serialised) {
  serialised_password_list_ = serialised;
  password_count_ = count;
  if (IsReadyForExport())
    Export();
}

void PasswordManagerExporter::Cancel() {
  // Tasks which had their pointers invalidated won't run.
  weak_factory_.InvalidateWeakPtrs();

  // If we are currently still serialising, Export() will see the cancellation
  // status and won't schedule writing.
  OnProgress(ExportProgressStatus::FAILED_CANCELLED, std::string());

  // If we are currently writing to the disk, we will have to cleanup the file
  // once writing stops.
  Cleanup();
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
  if (GetProgressStatus() == ExportProgressStatus::FAILED_CANCELLED) {
    serialised_password_list_.clear();
    return;
  }

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(DoWriteOnTaskRunner, write_function_,
                     set_permissions_function_, destination_,
                     std::move(serialised_password_list_)),
      base::BindOnce(&PasswordManagerExporter::OnPasswordsExported,
                     weak_factory_.GetWeakPtr()));
}

void PasswordManagerExporter::OnPasswordsExported(bool success) {
  if (success) {
    OnProgress(ExportProgressStatus::SUCCEEDED, std::string());
  } else {
    OnProgress(ExportProgressStatus::FAILED_WRITE_FAILED,
               destination_.DirName().BaseName().AsUTF8Unsafe());
    // Don't leave partial password files, if we tell the user we couldn't write
    Cleanup();
  }
}

void PasswordManagerExporter::OnProgress(ExportProgressStatus status,
                                         const std::string& folder) {
  last_progress_status_ = status;
  on_progress_.Run(status, folder);
}

void PasswordManagerExporter::Cleanup() {
  // The PasswordManagerExporter instance may be destroyed before the cleanup is
  // executed, e.g. because a new export was initiated. The cleanup should be
  // carried out regardless, so we only schedule tasks which own their
  // arguments.
  // TODO(crbug.com/811779) When Chrome is overwriting an existing file, cancel
  // should restore the file rather than delete it.
  if (!destination_.empty()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(delete_function_), destination_));
  }
}

}  // namespace password_manager
