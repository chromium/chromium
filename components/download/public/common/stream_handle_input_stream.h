// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_STREAM_HANDLE_INPUT_STREAM_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_STREAM_HANDLE_INPUT_STREAM_H_

#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_stream.mojom.h"
#include "components/download/public/common/input_stream.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace download {

// Download input stream backed by a DownloadStreamHandle.
class COMPONENTS_DOWNLOAD_EXPORT StreamHandleInputStream
    : public InputStream,
      public mojom::DownloadStreamClient {
 public:
  explicit StreamHandleInputStream(
      mojom::DownloadStreamHandlePtr stream_handle);
  ~StreamHandleInputStream() override;

  // InputStream
  void Initialize() override;
  bool IsEmpty() override;
  void RegisterDataReadyCallback(
      const mojo::SimpleWatcher::ReadyCallback& callback) override;
  void ClearDataReadyCallback() override;
  void RegisterCompletionCallback(base::OnceClosure callback) override;
  InputStream::StreamState Read(scoped_refptr<net::IOBuffer>* data,
                                          size_t* length) override;
  DownloadInterruptReason GetCompletionStatus() override;

  // mojom::DownloadStreamClient
  void OnStreamCompleted(mojom::NetworkRequestStatus status) override;

 private:
  // Objects for consuming a mojo data pipe.
  mojom::DownloadStreamHandlePtr stream_handle_;
  std::unique_ptr<mojo::SimpleWatcher> handle_watcher_;
  std::unique_ptr<mojo::Receiver<mojom::DownloadStreamClient>> receiver_;

  // Whether the producer has completed handling the response.
  bool is_response_completed_;

  // Status when the response completes, used by data pipe.
  DownloadInterruptReason completion_status_;

  // Callback to run when stream completes.
  base::OnceClosure completion_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(StreamHandleInputStream);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_STREAM_HANDLE_INPUT_STREAM_H_
