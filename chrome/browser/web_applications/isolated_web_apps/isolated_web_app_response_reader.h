// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_RESPONSE_READER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_RESPONSE_READER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"

namespace network {
struct ResourceRequest;
}

namespace web_app {

// The definition of the interface that is used to read responses for requests
// from Isolated Web Apps. Usually, the implementations of this interface
// should be constructed via the `IsolatedWebAppResponseReaderFactory`, which
// will take care of the necessary validation and verification steps.
class IsolatedWebAppResponseReader {
 public:
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

  struct Error {
    enum class Type {
      kParserInternalError,
      kFormatError,
      kResponseNotFound,
      kNotTrusted,
    };

    static Error FromSignedWebBundleReaderError(
        const SignedWebBundleReader::ReadResponseError& error);
    // Returns `base::ok` if the trust checker result indicates success. Returns
    // an error otherwise.
    static base::expected<void, Error> FromTrustCheckerResult(
        const IsolatedWebAppTrustChecker::Result& result);

    Type type;
    std::string message;

   private:
    Error(Type type, std::string message)
        : type(type), message(std::move(message)) {}
  };

  using ReadResponseCallback =
      base::OnceCallback<void(base::expected<Response, Error>)>;

  virtual ~IsolatedWebAppResponseReader() = default;

  virtual web_package::SignedWebBundleIntegrityBlock GetIntegrityBlock() = 0;
  virtual void ReadResponse(const network::ResourceRequest& resource_request,
                            ReadResponseCallback callback) = 0;
  virtual void Close(base::OnceClosure callback) = 0;
};

// The implementation of the IWA response reader. It is constructed from
// a `SignedWebBundleReader` instance, which must have already
// read and validated integrity block and metadata.
class IsolatedWebAppResponseReaderImpl : public IsolatedWebAppResponseReader {
 public:
  using TrustChecker =
      base::RepeatingCallback<IsolatedWebAppTrustChecker::Result()>;

  explicit IsolatedWebAppResponseReaderImpl(
      std::unique_ptr<SignedWebBundleReader> reader,
      TrustChecker trust_checker);
  ~IsolatedWebAppResponseReaderImpl() override;

  web_package::SignedWebBundleIntegrityBlock GetIntegrityBlock() override;
  void ReadResponse(const network::ResourceRequest& resource_request,
                    ReadResponseCallback callback) override;
  void Close(base::OnceClosure callback) override;

 private:
  void OnResponseRead(
      ReadResponseCallback callback,
      base::expected<web_package::mojom::BundleResponsePtr,
                     SignedWebBundleReader::ReadResponseError> response_head);
  void OnClosed(base::OnceClosure callback);

  std::unique_ptr<SignedWebBundleReader> reader_;
  TrustChecker trust_checker_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_RESPONSE_READER_H_
