// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_MANIFEST_DOWNLOADER_H_
#define COMPONENTS_NACL_RENDERER_MANIFEST_DOWNLOADER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

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

// Downloads a NaCl manifest (.nmf) and returns the contents of the file to
// caller through a callback.
class ManifestDownloader : public blink::WebAssociatedURLLoaderClient {
 public:
  typedef base::OnceCallback<void(PP_NaClError, const std::string&)> Callback;

  // This is a pretty arbitrary limit on the byte size of the NaCl manifest
  // file.
  // Note that the resulting string object has to have at least one byte extra
  // for the null termination character.
  static const size_t kNaClManifestMaxFileBytes = 1024 * 1024;

  ManifestDownloader(std::unique_ptr<blink::WebAssociatedURLLoader> url_loader,
                     bool is_installed,
                     Callback cb);
  ~ManifestDownloader() override;

  void Load(const blink::WebURLRequest& request);

 private:
  void Close();

  // WebAssociatedURLLoaderClient implementation.
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidReceiveData(base::span<const char> data) override;
  void DidFinishLoading() override;
  void DidFail(const blink::WebURLError& error) override;

  std::unique_ptr<blink::WebAssociatedURLLoader> url_loader_;
  bool is_installed_;
  Callback cb_;
  std::string buffer_;
  int status_code_;
  PP_NaClError pp_nacl_error_;
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_MANIFEST_DOWNLOADER_H_
