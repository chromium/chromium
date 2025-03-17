// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_SIGNED_WEB_BUNDLE_READER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_SIGNED_WEB_BUNDLE_READER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "net/base/net_errors.h"
#include "services/data_decoder/public/cpp/safe_web_bundle_parser.h"
#include "url/gurl.h"

namespace network {
struct ResourceRequest;
}  // namespace network

namespace web_app {

// This class is a reader for Signed Web Bundles.
//
// `Create` returns a new instance of this class.
//
// `ReadIntegrityBlock` starts the process to read the Signed Web Bundle's
// integrity block and metadata, as well as to verify that the signatures
// contained in the integrity block sign the bundle correctly.
//
// If everything is parsed successfully, then
// the caller can make requests to responses contained in the Signed Web Bundle
// using `ReadResponse` and `ReadResponseBody`. The caller can then also access
// the metadata contained in the Signed Web Bundle. Potential errors occurring
// during initialization are irrecoverable. Whether initialization has completed
// can be determined by either waiting for the callback passed to `StartReading`
// to run or by querying `GetState`.
//
// URLs passed to `ReadResponse` will be simplified to remove username,
// password, and fragment before looking up the corresponding response inside
// the Signed Web Bundle. This is the same behavior as with unsigned Web
// Bundles (see `content::WebBundleReader`).
class SignedWebBundleReader {
 public:
  using Result = base::expected<std::unique_ptr<SignedWebBundleReader>,
                                UnusableSwbnFileError>;
  using CreateCallback = base::OnceCallback<void(Result)>;

  // Creates a new instance of this class. `base_url` is used inside the
  // `WebBundleParser` to convert relative URLs contained in the Web Bundle into
  // absolute URLs. If `base_url` is `std::nullopt`, then relative URLs inside
  // the Web Bundle will result in an error.
  static void Create(const base::FilePath& web_bundle_path,
                     const std::optional<GURL>& base_url,
                     bool verify_signatures,
                     CreateCallback callback);

  static base::AutoReset<web_package::SignedWebBundleSignatureVerifier*>
  SetSignatureVerifierForTesting(
      web_package::SignedWebBundleSignatureVerifier*);

  SignedWebBundleReader() = default;
  virtual ~SignedWebBundleReader() = default;

  SignedWebBundleReader(const SignedWebBundleReader&) = delete;
  SignedWebBundleReader& operator=(const SignedWebBundleReader&) = delete;

  // Closes all the closable resources that the reader is using.
  virtual void Close(base::OnceClosure callback) = 0;

  virtual bool IsClosed() const = 0;

  // Returns the integrity block of the Web Bundle.
  virtual const web_package::SignedWebBundleIntegrityBlock& GetIntegrityBlock()
      const = 0;

  // Returns the primary URL, as specified in the metadata of the Web Bundle.
  virtual const std::optional<GURL>& GetPrimaryURL() const = 0;

  // Returns the URLs of all exchanges contained in the Web Bundle, as specified
  // in the metadata.
  virtual std::vector<GURL> GetEntries() const = 0;

  struct ReadResponseError {
    enum class Type {
      kParserInternalError,
      kFormatError,
      kResponseNotFound,
    };

    static ReadResponseError FromBundleParseError(
        web_package::mojom::BundleResponseParseErrorPtr error);
    static ReadResponseError ForParserInternalError(const std::string& message);
    static ReadResponseError ForResponseNotFound(const std::string& message);

    Type type;
    std::string message;

   private:
    ReadResponseError(Type type, const std::string& message)
        : type(type), message(message) {}
  };

  // Reads the status code and headers, as well as the length and offset of the
  // response body within the Web Bundle. The URL will be simplified
  // (credentials and fragment and removed, this is consistent with
  // `content::WebBundleReader`) before matching it to a response.
  using ResponseCallback = base::OnceCallback<void(
      base::expected<web_package::mojom::BundleResponsePtr,
                     ReadResponseError>)>;
  virtual void ReadResponse(const network::ResourceRequest& resource_request,
                            ResponseCallback callback) = 0;

  // Reads the response body given a `response` read with `ReadResponse`.
  using ResponseBodyCallback = base::OnceCallback<void(net::Error net_error)>;
  virtual void ReadResponseBody(
      web_package::mojom::BundleResponsePtr response,
      mojo::ScopedDataPipeProducerHandle producer_handle,
      ResponseBodyCallback callback) = 0;

  virtual base::WeakPtr<SignedWebBundleReader> AsWeakPtr() = 0;
};

// This is a base class for fetching an info about a unsecure .swbn file.
// The implementation of the pure virtual functions of this class should
// provide a logic to read a specific thing from a bundle.
// A signed web bundle considered unsecure if the signed web bundle ID of the
// file is not known from a trusted source. Examples of trusted source of the ID
// are the enterprise policy, a distributor store, etc.
// Integrity check of the .swbn file without knowing the expected ID makes no
// sense as an attacker can resign the tampered bundle with their private key.
class UnsecureReader {
 public:
  UnsecureReader(const UnsecureReader&) = delete;
  UnsecureReader& operator=(const UnsecureReader&) = delete;
  virtual ~UnsecureReader();

 protected:
  explicit UnsecureReader(const base::FilePath& web_bundle_path);
  // Initializes the connection which in the end leads to
  // |DoReading()| execution.
  void StartReading();

  // Implementation of this does real work to fetch the particular
  // information about the .swbn file.
  virtual void DoReading() = 0;
  // Implementation should return an error to the caller.
  virtual void ReturnError(UnusableSwbnFileError error) = 0;
  virtual base::WeakPtr<UnsecureReader> GetWeakPtr() = 0;

  void OnFileOpened(base::File file);

  base::FilePath web_bundle_path_;
  std::unique_ptr<data_decoder::SafeWebBundleParser> parser_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class UnsecureSignedWebBundleIdReader : public UnsecureReader {
 public:
  using WebBundleIdCallback = base::OnceCallback<void(
      base::expected<web_package::SignedWebBundleId, UnusableSwbnFileError>)>;

  static void GetWebBundleId(const base::FilePath& web_bundle_path,
                             WebBundleIdCallback result_callback);

  ~UnsecureSignedWebBundleIdReader() override;

 protected:
  explicit UnsecureSignedWebBundleIdReader(
      const base::FilePath& web_bundle_path);

  void DoReading() override;
  base::WeakPtr<UnsecureReader> GetWeakPtr() override;
  void ReturnError(UnusableSwbnFileError error) override;

  void OnIntegrityBlockParsed(
      web_package::mojom::BundleIntegrityBlockPtr raw_integrity_block,
      web_package::mojom::BundleIntegrityBlockParseErrorPtr error);

 private:
  void SetResultCallback(WebBundleIdCallback web_bundle_id_result_callback);

  WebBundleIdCallback web_bundle_id_callback_;
  base::WeakPtrFactory<UnsecureSignedWebBundleIdReader> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_SIGNED_WEB_BUNDLE_READER_H_
