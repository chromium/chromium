// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_BACKGROUND_DOWNLOADER_MAC_H_
#define COMPONENTS_UPDATE_CLIENT_BACKGROUND_DOWNLOADER_MAC_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/update_client/crx_downloader.h"

namespace update_client {
namespace {
// TODO(crbug.com/40285933): The session identifier might need to be more
// complex to accommodate multiple Chrome processes.
inline constexpr char kDefaultBackgroundSessionId[] = "CrxDownloader";
}  // namespace

class BackgroundDownloaderSharedSession
    : public base::RefCountedThreadSafe<BackgroundDownloaderSharedSession> {
 public:
  using OnDownloadCompleteCallback = base::RepeatingCallback<void(
      bool,
      const update_client::CrxDownloader::Result&,
      const update_client::CrxDownloader::DownloadMetrics&)>;

  virtual void DoStartDownload(
      const GURL& url,
      OnDownloadCompleteCallback on_download_complete_callback) = 0;

  // Cancel all download tasks and invalidates this session. Future calls to
  // DoStartDownload will fail until the session is recreated.
  virtual void InvalidateAndCancel() = 0;

 protected:
  friend class base::RefCountedThreadSafe<BackgroundDownloaderSharedSession>;
  virtual ~BackgroundDownloaderSharedSession() = default;
};

class BackgroundDownloader : public CrxDownloader {
 public:
  BackgroundDownloader(
      scoped_refptr<CrxDownloader> successor,
      scoped_refptr<BackgroundDownloaderSharedSession> shared_session,
      scoped_refptr<base::SequencedTaskRunner> background_sequence_);

 private:
  friend class BackgroundDownloaderTest;
  friend class BackgroundDownloaderCrashingClientTest;
  FRIEND_TEST_ALL_PREFIXES(BackgroundDownloaderTest, ConcurrentDownloaders);

  // Overrides for CrxDownloader.
  ~BackgroundDownloader() override;
  base::OnceClosure DoStartDownload(const GURL& url) override;

  base::OnceClosure DoStartDownload(
      const GURL& url,
      BackgroundDownloaderSharedSession::OnDownloadCompleteCallback);

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<BackgroundDownloaderSharedSession> shared_session_;
  scoped_refptr<base::SequencedTaskRunner> background_sequence_;
  base::WeakPtrFactory<BackgroundDownloader> weak_ptr_factory_{this};
};

scoped_refptr<BackgroundDownloaderSharedSession>
MakeBackgroundDownloaderSharedSession(
    scoped_refptr<base::SequencedTaskRunner> background_sequence,
    const base::FilePath& download_cache,
    const std::string& session_identifier = kDefaultBackgroundSessionId);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_BACKGROUND_DOWNLOADER_MAC_H_
