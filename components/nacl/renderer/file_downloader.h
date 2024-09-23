// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_FILE_DOWNLOADER_H_
#define COMPONENTS_NACL_RENDERER_FILE_DOWNLOADER_H_

#include <stdint.h>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "components/nacl/renderer/ppb_nacl_private.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"

namespace blink {
class WebAssociatedURLLoader;
struct WebURLError;
class WebURLRequest;
class WebURLResponse;
}

namespace nacl {

// Downloads a file and writes the contents to a specified file open for
// writing.
class FileDownloader : public blink::WebAssociatedURLLoaderClient {
 public:
  enum Status {
    SUCCESS,
    ACCESS_DENIED,
    FAILED
  };

  // Provides the FileDownloader status and the HTTP status code.
  typedef base::OnceCallback<void(Status, base::File, int)> StatusCallback;

  // Provides the bytes received so far, and the total bytes expected to be
  // received.
  typedef base::RepeatingCallback<void(int64_t, int64_t)> ProgressCallback;

  FileDownloader(std::unique_ptr<blink::WebAssociatedURLLoader> url_loader,
                 base::File file,
                 StatusCallback status_cb,
                 ProgressCallback progress_cb);

  ~FileDownloader() override;

  void Load(const blink::WebURLRequest& request);

 private:
  // WebAssociatedURLLoaderClient implementation.
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidReceiveData(base::span<const char> data) override;
  void DidFinishLoading() override;
  void DidFail(const blink::WebURLError& error) override;

  std::unique_ptr<blink::WebAssociatedURLLoader> url_loader_;
  base::File file_;
  StatusCallback status_cb_;
  ProgressCallback progress_cb_;
  int http_status_code_;
  int64_t total_bytes_received_;
  int64_t total_bytes_to_be_received_;
  Status status_;
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_FILE_DOWNLOADER_H_
