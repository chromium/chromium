// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IOS_BACKGROUND_DOWNLOAD_TASK_HELPER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IOS_BACKGROUND_DOWNLOAD_TASK_HELPER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"

namespace base {
class FilePath;
}  // namespace base

namespace download {
struct RequestParams;
struct SchedulingParams;

// Helper class to perform background download with iOS platform API.
// Notes:
// 1. Needs to debug on real device, with Xcode debugger detached to make the
// device enter background session.
// 2. If the user kills the app in multitask window, the session is deleted, the
// app will not be waked up to resume the download.
class BackgroundDownloadTaskHelper {
 public:
  // Callback with whether download is succeeded and the file path and the file
  // size of the succeeded download.
  using CompletionCallback =
      base::OnceCallback<void(bool, const base::FilePath&, int64_t)>;
  // Callback with number of bytes downloaded.
  using UpdateCallback = base::RepeatingCallback<void(int64_t)>;
  static std::unique_ptr<BackgroundDownloadTaskHelper> Create();

  // For test only:
  // Ignore the SSL errors for request https://127.0.0.1/...
  // that are used to access EmbeddedTestServer.
  // This will allow features that use BackgroundDownloadService
  // to be tested using EmbeddedTestServer.
  static void SetIgnoreLocalSSLErrorForTesting(bool ignore);

  BackgroundDownloadTaskHelper() = default;
  virtual ~BackgroundDownloadTaskHelper() = default;
  BackgroundDownloadTaskHelper(const BackgroundDownloadTaskHelper&) = delete;
  BackgroundDownloadTaskHelper& operator=(const BackgroundDownloadTaskHelper&) =
      delete;

  // Starts a download.
  virtual void StartDownload(const std::string& guid,
                             const base::FilePath& target_path,
                             const RequestParams& request_params,
                             const SchedulingParams& scheduling_params,
                             CompletionCallback completion_callback,
                             UpdateCallback update_callback) = 0;

  // Called to handle events for background download.
  virtual void HandleEventsForBackgroundURLSession(
      base::OnceClosure completion_handler) {}
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IOS_BACKGROUND_DOWNLOAD_TASK_HELPER_H_
