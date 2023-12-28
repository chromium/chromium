// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUIRKS_QUIRKS_CLIENT_H_
#define COMPONENTS_QUIRKS_QUIRKS_CLIENT_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"

namespace network {
class SimpleURLLoader;
}

namespace quirks {

class QuirksManager;

// See declaration in quirks_manager.h.
using RequestFinishedCallback =
    base::OnceCallback<void(const base::FilePath&, bool)>;

// Handles downloading icc and other display data files from Quirks Server.
class QuirksClient {
 public:
  QuirksClient(int64_t product_id,
               const std::string& display_name,
               RequestFinishedCallback on_request_finished,
               QuirksManager* manager);

  QuirksClient(const QuirksClient&) = delete;
  QuirksClient& operator=(const QuirksClient&) = delete;

  ~QuirksClient();

  void StartDownload();

  int64_t product_id() const { return product_id_; }

 private:
  void OnDownloadComplete(std::unique_ptr<std::string> response_body);

  // Send callback and tell manager to delete |this|.
  void Shutdown(bool success);

  // Schedules a retry.
  void Retry();

  // Translates json with base64-encoded data (|result|) into raw |data|.
  bool ParseResult(const std::string& result, std::string* data);

  // ID of display to request from Quirks Server.
  const int64_t product_id_;

  // Human-readable name to send to Quirks Server.
  const std::string display_name_;

  // Callback supplied by caller.
  RequestFinishedCallback on_request_finished_;

  // Weak pointer owned by manager, guaranteed to outlive this client object.
  raw_ptr<QuirksManager> manager_;

  // Full path to icc file.
  const base::FilePath icc_path_;

  // The class is expected to run on UI thread.
  base::ThreadChecker thread_checker_;

  // This loader is used to download icc file.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Pending retry.
  base::OneShotTimer request_scheduled_;

  // Controls exponential backoff of time between server checks.
  net::BackoffEntry backoff_entry_;

  // Factory for callbacks.
  base::WeakPtrFactory<QuirksClient> weak_ptr_factory_{this};
};

}  // namespace quirks

#endif  // COMPONENTS_QUIRKS_QUIRKS_CLIENT_H_
