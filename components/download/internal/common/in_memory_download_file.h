// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_COMMON_IN_MEMORY_DOWNLOAD_FILE_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_COMMON_IN_MEMORY_DOWNLOAD_FILE_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/download/public/common/download_file.h"
#include "components/download/public/common/download_save_info.h"
#include "components/download/public/common/rate_estimator.h"

namespace download {
class DownloadDestinationObserver;

// Implementation of DownloadFile that stores data in memory.
class COMPONENTS_DOWNLOAD_EXPORT InMemoryDownloadFile : public DownloadFile {
 public:
  InMemoryDownloadFile(std::unique_ptr<DownloadSaveInfo> save_info,
                       std::unique_ptr<InputStream> stream,
                       base::WeakPtr<DownloadDestinationObserver> observer);
  InMemoryDownloadFile(const InMemoryDownloadFile&) = delete;
  InMemoryDownloadFile& operator=(const InMemoryDownloadFile&) = delete;
  ~InMemoryDownloadFile() override;

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
  void PublishDownload(RenameCompletionCallback callback) override;
  bool IsMemoryFile() override;

 private:
  void SendUpdate();
  void StreamActive(MojoResult result);
  void OnStreamCompleted();
  void NotifyObserver(DownloadInterruptReason reason,
                      InputStream::StreamState stream_state);

  // DownloadSaveInfo provided during construction. Since the DownloadFileImpl
  // can be created on any thread, this holds the save_info_ until it can be
  // used to initialize file_ on the download sequence.
  std::unique_ptr<DownloadSaveInfo> save_info_;

  std::unique_ptr<InputStream> input_stream_;

  // TaskRunner to post updates to the |observer_|.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Reference to the Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  RateEstimator rate_estimator_;

  int64_t total_bytes_ = 0;

  // Path to the memory file.
  base::FilePath memory_file_path_;

  base::WeakPtr<DownloadDestinationObserver> observer_;
  base::WeakPtrFactory<InMemoryDownloadFile> weak_factory_{this};
};

}  //  namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_COMMON_IN_MEMORY_DOWNLOAD_FILE_H_
