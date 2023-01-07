// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_READER_REGISTRY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_READER_REGISTRY_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_signature_verifier.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_package {
class SignedWebBundleId;
}  // namespace web_package

namespace web_app {

// A registry to create and keep track of `SignedWebBundleReader` instances used
// to read Isolated Web Apps. At its core, it contains a map from file paths to
// `SignedWebBundleReader`s to cache them for repeated calls. On non-ChromeOS
// devices, the first request for a particular file path will also check the
// integrity of the Signed Web Bundle. On ChromeOS, it is assumed that the
// Signed Web Bundle has not been corrupted due to its location inside
// cryptohome, and signatures are not checked.
class IsolatedWebAppReaderRegistry : public KeyedService {
 public:
  explicit IsolatedWebAppReaderRegistry(
      std::unique_ptr<IsolatedWebAppValidator> validator,
      base::RepeatingCallback<
          std::unique_ptr<SignedWebBundleSignatureVerifier>()>
          signature_verifier_factory);
  ~IsolatedWebAppReaderRegistry() override;

  IsolatedWebAppReaderRegistry(const IsolatedWebAppReaderRegistry&) = delete;
  IsolatedWebAppReaderRegistry& operator=(const IsolatedWebAppReaderRegistry&) =
      delete;

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
    // may also occur if the `SignedWebBundleReader` that was used to read the
    // response head has since been evicted from the cache.
    void ReadBody(mojo::ScopedDataPipeProducerHandle producer_handle,
                  base::OnceCallback<void(net::Error net_error)> callback);

   private:
    web_package::mojom::BundleResponsePtr head_;
    base::WeakPtr<SignedWebBundleReader> reader_;
  };

  struct ReadResponseError {
    enum class Type {
      kOtherError,
      kResponseNotFound,
    };

    static ReadResponseError ForOtherError(const std::string& message) {
      return ReadResponseError(Type::kOtherError, message);
    }

    static ReadResponseError ForResponseNotFound(const std::string& message) {
      return ReadResponseError(Type::kResponseNotFound, message);
    }

    Type type;
    std::string message;

   private:
    ReadResponseError(Type type, const std::string& message)
        : type(type), message(message) {}
  };

  using ReadResponseCallback = base::OnceCallback<void(
      base::expected<Response, ReadResponseError> response)>;

  // Given a path to a Signed Web Bundle, the expected Signed Web Bundle ID, and
  // a request, read the corresponding response from it. The `callback` receives
  // both the response head and a closure it can call to read the response body,
  // or a string if an error occurs.
  void ReadResponse(const base::FilePath& web_bundle_path,
                    const web_package::SignedWebBundleId& web_bundle_id,
                    const network::ResourceRequest& resource_request,
                    ReadResponseCallback callback);

 private:
  void OnIntegrityBlockRead(
      const base::FilePath& web_bundle_path,
      const web_package::SignedWebBundleId& web_bundle_id,
      const std::vector<web_package::Ed25519PublicKey>& public_key_stack,
      base::OnceCallback<
          void(SignedWebBundleReader::SignatureVerificationAction)> callback);

  void OnIntegrityBlockAndMetadataRead(
      const base::FilePath& web_bundle_path,
      const web_package::SignedWebBundleId& web_bundle_id,
      absl::optional<SignedWebBundleReader::ReadError> read_error);

  void DoReadResponse(SignedWebBundleReader& reader,
                      network::ResourceRequest resource_request,
                      ReadResponseCallback callback);

  void OnResponseRead(
      SignedWebBundleReader& reader,
      ReadResponseCallback callback,
      base::expected<web_package::mojom::BundleResponsePtr,
                     SignedWebBundleReader::ReadResponseError> response_head);

  // A `CacheEntry` has two states: In its initial `kPending` state, it caches
  // requests made to a Signed Web Bundle until the `SignedWebBundleReader` is
  // ready. Once the `SignedWebBundleReader` is ready to serve responses, all
  // queued requests are run and the state is updated to `kReady`.
  struct CacheEntry {
    explicit CacheEntry(std::unique_ptr<SignedWebBundleReader> reader);
    ~CacheEntry();

    CacheEntry(const CacheEntry& other) = delete;
    CacheEntry& operator=(const CacheEntry& other) = delete;

    CacheEntry(CacheEntry&& other);
    CacheEntry& operator=(CacheEntry&& other);

    enum class State { kPending, kReady };

    State state = State::kPending;
    std::vector<std::pair<network::ResourceRequest, ReadResponseCallback>>
        pending_requests;
    std::unique_ptr<SignedWebBundleReader> reader;
  };

  base::flat_map<base::FilePath, CacheEntry> reader_cache_;

  std::unique_ptr<IsolatedWebAppValidator> validator_;
  base::RepeatingCallback<std::unique_ptr<SignedWebBundleSignatureVerifier>()>
      signature_verifier_factory_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_READER_REGISTRY_H_
