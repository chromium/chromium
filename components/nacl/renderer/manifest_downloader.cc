// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/manifest_downloader.h"

#include <utility>

#include "base/functional/callback.h"
#include "components/nacl/renderer/histogram.h"
#include "components/nacl/renderer/nexe_load_manager.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"

namespace nacl {

ManifestDownloader::ManifestDownloader(
    std::unique_ptr<blink::WebAssociatedURLLoader> url_loader,
    bool is_installed,
    Callback cb)
    : url_loader_(std::move(url_loader)),
      is_installed_(is_installed),
      cb_(std::move(cb)),
      status_code_(-1),
      pp_nacl_error_(PP_NACL_ERROR_LOAD_SUCCESS) {
  CHECK(!cb_.is_null());
}

ManifestDownloader::~ManifestDownloader() { }

void ManifestDownloader::Load(const blink::WebURLRequest& request) {
  url_loader_->LoadAsynchronously(request, this);
}

void ManifestDownloader::DidReceiveResponse(
    const blink::WebURLResponse& response) {
  if (response.HttpStatusCode() != 200)
    pp_nacl_error_ = PP_NACL_ERROR_MANIFEST_LOAD_URL;
  status_code_ = response.HttpStatusCode();
}

void ManifestDownloader::DidReceiveData(base::span<const char> data) {
  if (buffer_.size() + data.size() > kNaClManifestMaxFileBytes) {
    pp_nacl_error_ = PP_NACL_ERROR_MANIFEST_TOO_LARGE;
    buffer_.clear();
  }

  if (pp_nacl_error_ == PP_NACL_ERROR_LOAD_SUCCESS)
    buffer_.append(data.data(), data.size());
}

void ManifestDownloader::Close() {
  // We log the status code here instead of in didReceiveResponse so that we
  // always log a histogram value, even when we never receive a status code.
  HistogramHTTPStatusCode(
      is_installed_ ? "NaCl.HttpStatusCodeClass.Manifest.InstalledApp" :
                      "NaCl.HttpStatusCodeClass.Manifest.NotInstalledApp",
      status_code_);

  std::move(cb_).Run(pp_nacl_error_, buffer_);
  delete this;
}

void ManifestDownloader::DidFinishLoading() {
  Close();
}

void ManifestDownloader::DidFail(const blink::WebURLError& error) {
  // TODO(teravest): Find a place to share this code with PepperURLLoaderHost.
  pp_nacl_error_ = PP_NACL_ERROR_MANIFEST_LOAD_URL;
  switch (error.reason()) {
    case net::ERR_ACCESS_DENIED:
    case net::ERR_NETWORK_ACCESS_DENIED:
      pp_nacl_error_ = PP_NACL_ERROR_MANIFEST_NOACCESS_URL;
      break;
  }

  if (error.is_web_security_violation())
    pp_nacl_error_ = PP_NACL_ERROR_MANIFEST_NOACCESS_URL;

  Close();
}

}  // namespace nacl
