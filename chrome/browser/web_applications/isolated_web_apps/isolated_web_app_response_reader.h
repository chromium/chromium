// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_RESPONSE_READER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_RESPONSE_READER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"

namespace network {
struct ResourceRequest;
}

namespace web_app {

// This class is used to read responses for requests from Isolated Web Apps. It
// is constructed from a `SignedWebBundleReader` instance, which must have
// already read and validated integrity block and metadata. Usually, this class
// should be constructed via the `IsolatedWebAppResponseReaderFactory`, which
// will take care of the necessary validation and verification steps.
class IsolatedWebAppResponseReader {
 public:
  explicit IsolatedWebAppResponseReader(
      std::unique_ptr<SignedWebBundleReader> reader);
  ~IsolatedWebAppResponseReader();

  // A `Response` object contains the response head, as well as a `ReadBody`
  // function to read the response's body. It holds weakly onto a
  // `SignedWebBundleReader` for reading the response body. This reference will
  // remain valid until the reader is evicted from the cache of the
  // `IsolatedWebAppReaderRegistry`.
  class Response {
   public:
    Response(web_package::mojom::BundleResponsePtr head,
             base::WeakPtr<SignedWebBundleReader> reader);

    Response(const Response&) = delete;
    Response& operator=(const Response&) = delete;

    Response(Response&&);
    Response& operator=(Response&&);

    ~Response();

    // Returns the head of the response, which includes status code and response
    // headers.
    const web_package::mojom::BundleResponsePtr& head() { return head_; }

    // Reads the body of the response into `producer_handle`, calling `callback`
    // with `net::OK` on success, and another error code on failure. A failure
    // may also occur if the `IsolatedWebAppResponseReader` that was used to
    // read the response head has since been deleted.
    void ReadBody(mojo::ScopedDataPipeProducerHandle producer_handle,
                  base::OnceCallback<void(net::Error net_error)> callback);

   private:
    web_package::mojom::BundleResponsePtr head_;
    base::WeakPtr<SignedWebBundleReader> reader_;
  };

  using Error = SignedWebBundleReader::ReadResponseError;

  using ReadResponseCallback =
      base::OnceCallback<void(base::expected<Response, Error>)>;

  void ReadResponse(const network::ResourceRequest& resource_request,
                    ReadResponseCallback callback);

 private:
  void OnResponseRead(ReadResponseCallback callback,
                      base::expected<web_package::mojom::BundleResponsePtr,
                                     Error> response_head);

  std::unique_ptr<SignedWebBundleReader> reader_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_RESPONSE_READER_H_
