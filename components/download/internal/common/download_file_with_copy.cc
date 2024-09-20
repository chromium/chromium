// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/download_file_with_copy.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/download/database/download_db.h"
#include "components/download/database/download_db_conversions.h"
#include "components/download/database/download_db_entry.h"
#include "components/download/public/common/download_destination_observer.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_utils.h"
#include "crypto/secure_hash.h"

namespace download {

namespace {
void OnRenameComplete(const base::FilePath& file_path,
                      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                      DownloadFile::RenameCompletionCallback callback) {
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                DOWNLOAD_INTERRUPT_REASON_NONE, file_path));
}
}  // namespace

DownloadFileWithCopy::DownloadFileWithCopy(
    const base::FilePath& file_path_to_copy,
    base::WeakPtr<DownloadDestinationObserver> observer)
    : file_path_to_copy_(file_path_to_copy),
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      observer_(observer) {}

DownloadFileWithCopy::~DownloadFileWithCopy() = default;

void DownloadFileWithCopy::Initialize(
    InitializeCallback initialize_callback,
    CancelRequestCallback cancel_request_callback,
    const DownloadItem::ReceivedSlices& received_slices) {
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(initialize_callback),
                                DOWNLOAD_INTERRUPT_REASON_NONE, 0));
}

void DownloadFileWithCopy::AddInputStream(std::unique_ptr<InputStream> stream,
                                          int64_t offset) {}

void DownloadFileWithCopy::RenameAndUniquify(
    const base::FilePath& full_path,
    RenameCompletionCallback callback) {
  base::File file(file_path_to_copy_,
                  base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ);
  int64_t file_size = file.GetLength();
  OnRenameComplete(full_path, main_task_runner_, std::move(callback));

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadDestinationObserver::DestinationCompleted,
                     observer_, file_size, nullptr));
}

void DownloadFileWithCopy::RenameAndAnnotate(
    const base::FilePath& full_path,
    const std::string& client_guid,
    const GURL& source_url,
    const GURL& referrer_url,
    const std::optional<url::Origin>& request_initiator,
    mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine,
    RenameCompletionCallback callback) {
  if (full_path != file_path_to_copy_) {
    base::CopyFile(file_path_to_copy_, full_path);
  }

  OnRenameComplete(full_path, main_task_runner_, std::move(callback));
}

void DownloadFileWithCopy::Detach() {}

void DownloadFileWithCopy::Cancel() {}

void DownloadFileWithCopy::SetPotentialFileLength(int64_t length) {}

const base::FilePath& DownloadFileWithCopy::FullPath() const {
  return file_path_to_copy_;
}

bool DownloadFileWithCopy::InProgress() const {
  return true;
}

void DownloadFileWithCopy::Pause() {}

void DownloadFileWithCopy::Resume() {}

#if BUILDFLAG(IS_ANDROID)
void DownloadFileWithCopy::PublishDownload(RenameCompletionCallback callback) {
  // This shouldn't get called.
  DCHECK(false);
}
#endif  // BUILDFLAG(IS_ANDROID)
}  //  namespace download
