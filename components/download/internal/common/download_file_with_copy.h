// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_COMMON_DOWNLOAD_FILE_WITH_COPY_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_COMMON_DOWNLOAD_FILE_WITH_COPY_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/download/public/common/download_file.h"

namespace download {
class DownloadDestinationObserver;

// Implementation of DownloadFile that copies another file to the given
// destination.
class COMPONENTS_DOWNLOAD_EXPORT DownloadFileWithCopy : public DownloadFile {
 public:
  DownloadFileWithCopy(const base::FilePath& file_path_to_copy,
                       base::WeakPtr<DownloadDestinationObserver> observer);
  DownloadFileWithCopy(const DownloadFileWithCopy&) = delete;
  DownloadFileWithCopy& operator=(const DownloadFileWithCopy&) = delete;
  ~DownloadFileWithCopy() override;

  // DownloadFile functions.
  void Initialize(InitializeCallback initialize_callback,
                  CancelRequestCallback cancel_request_callback,
                  const DownloadItem::ReceivedSlices& received_slices) override;
  void AddInputStream(std::unique_ptr<InputStream> stream,
                      int64_t offset) override;
  void RenameAndUniquify(const base::FilePath& full_path,
                         RenameCompletionCallback callback) override;
  void RenameAndAnnotate(
      const base::FilePath& full_path,
      const std::string& client_guid,
      const GURL& source_url,
      const GURL& referrer_url,
      const std::optional<url::Origin>& request_initiator,
      mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine,
      RenameCompletionCallback callback) override;
  void Detach() override;
  void Cancel() override;
  void SetPotentialFileLength(int64_t length) override;
  const base::FilePath& FullPath() const override;
  bool InProgress() const override;
  void Pause() override;
  void Resume() override;
#if BUILDFLAG(IS_ANDROID)
  void PublishDownload(RenameCompletionCallback callback) override;
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  // Whether this object has already been initialized.
  base::FilePath file_path_to_copy_;

  // TaskRunner to post updates to the |observer_|.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  base::WeakPtr<DownloadDestinationObserver> observer_;
  base::WeakPtrFactory<DownloadFileWithCopy> weak_factory_{this};
};

}  //  namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_COMMON_DOWNLOAD_FILE_WITH_COPY_H_
