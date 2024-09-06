// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/file_downloader.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "components/nacl/renderer/nexe_load_manager.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"

namespace nacl {

FileDownloader::FileDownloader(
    std::unique_ptr<blink::WebAssociatedURLLoader> url_loader,
    base::File file,
    StatusCallback status_cb,
    ProgressCallback progress_cb)
    : url_loader_(std::move(url_loader)),
      file_(std::move(file)),
      status_cb_(std::move(status_cb)),
      progress_cb_(std::move(progress_cb)),
      http_status_code_(-1),
      total_bytes_received_(0),
      total_bytes_to_be_received_(-1),
      status_(SUCCESS) {
  CHECK(!status_cb_.is_null());
}

FileDownloader::~FileDownloader() {
}

void FileDownloader::Load(const blink::WebURLRequest& request) {
  url_loader_->LoadAsynchronously(request, this);
}

void FileDownloader::DidReceiveResponse(const blink::WebURLResponse& response) {
  http_status_code_ = response.HttpStatusCode();
  if (http_status_code_ != 200)
    status_ = FAILED;

  // Set -1 if the content length is unknown. Set before issuing callback.
  total_bytes_to_be_received_ = response.ExpectedContentLength();
  if (!progress_cb_.is_null())
    progress_cb_.Run(total_bytes_received_, total_bytes_to_be_received_);
}

void FileDownloader::DidReceiveData(base::span<const char> data) {
  if (status_ == SUCCESS) {
    if (file_.Write(total_bytes_received_, base::as_bytes(data)) == -1) {
      status_ = FAILED;
      return;
    }
    total_bytes_received_ += data.size();
    if (!progress_cb_.is_null())
      progress_cb_.Run(total_bytes_received_, total_bytes_to_be_received_);
  }
}

void FileDownloader::DidFinishLoading() {
  if (status_ == SUCCESS) {
    // Seek back to the beginning of the file that was just written so it's
    // easy for consumers to use.
    if (file_.Seek(base::File::FROM_BEGIN, 0) != 0)
      status_ = FAILED;
  }
  std::move(status_cb_).Run(status_, std::move(file_), http_status_code_);
  delete this;
}

void FileDownloader::DidFail(const blink::WebURLError& error) {
  status_ = FAILED;
  switch (error.reason()) {
    case net::ERR_ACCESS_DENIED:
    case net::ERR_NETWORK_ACCESS_DENIED:
      status_ = ACCESS_DENIED;
      break;
  }

  if (error.is_web_security_violation())
    status_ = ACCESS_DENIED;

  // Delete url_loader to prevent didFinishLoading from being called, which
  // some implementations of blink::URLLoader will do after calling didFail.
  url_loader_.reset();

  std::move(status_cb_).Run(status_, std::move(file_), http_status_code_);
  delete this;
}

}  // namespace nacl
