// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_SIGNED_WEB_BUNDLE_READER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_SIGNED_WEB_BUNDLE_READER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "net/base/net_errors.h"
#include "services/data_decoder/public/cpp/safe_web_bundle_parser.h"
#include "url/gurl.h"

namespace network {
struct ResourceRequest;
}

namespace mojo {
class DataPipeProducer;
}  // namespace mojo

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
  // Callers of this class can decide whether parsing the Signed Web Bundle
  // should continue or stop after the integrity block has been read by passing
  // an appropriate instance of this class to the
  // `integrity_block_result_callback`. If a caller decides that parsing should
  // stop, then metadata will not be read and the `read_error_callback` will run
  // with an `AbortedByCaller` error.
  class SignatureVerificationAction {
   public:
    enum class Type {
      kAbort,
      kContinueAndVerifySignatures,
      kContinueAndSkipSignatureVerification,
    };

    static SignatureVerificationAction Abort(UnusableSwbnFileError error);
    static SignatureVerificationAction ContinueAndVerifySignatures();
    static SignatureVerificationAction ContinueAndSkipSignatureVerification();

    SignatureVerificationAction(const SignatureVerificationAction&);
    ~SignatureVerificationAction();

    Type type() { return type_; }

    // Will CHECK if `type()` != `Type::kAbort`.
    UnusableSwbnFileError error() { return *error_; }

   private:
    SignatureVerificationAction(Type type,
                                std::optional<UnusableSwbnFileError> error);

    const Type type_;
    const std::optional<UnusableSwbnFileError> error_;
  };

  using IntegrityBlockReadResultCallback = base::OnceCallback<void(
      base::expected<web_package::SignedWebBundleIntegrityBlock,
                     UnusableSwbnFileError>)>;

  using ReadErrorCallback = base::OnceCallback<void(
      base::expected<void, UnusableSwbnFileError> status)>;

  // Creates a new instance of this class. `base_url` is used inside the
  // `WebBundleParser` to convert relative URLs contained in the Web Bundle into
  // absolute URLs. If `base_url` is `std::nullopt`, then relative URLs inside
  // the Web Bundle will result in an error.
  static std::unique_ptr<SignedWebBundleReader> Create(
      const base::FilePath& web_bundle_path,
      const std::optional<GURL>& base_url,
      std::unique_ptr<
          web_package::SignedWebBundleSignatureVerifier> signature_verifier =
          std::make_unique<web_package::SignedWebBundleSignatureVerifier>());

  // Starts reading the Signed Web Bundle. This will invoke
  // `integrity_block_result_callback` after reading the integrity block, which
  // must then, based on the public keys contained in the integrity block,
  // determine whether this class should continue with signature verification
  // and metadata reading, or abort. In any case,
  // `read_error_callback` will be called once reading integrity block and
  // metadata has either succeeded, was aborted, or failed.
  // Will CHECK if `GetState()` != `kUninitialized`.
  void ReadIntegrityBlock(
      IntegrityBlockReadResultCallback integrity_block_result_callback);

  void ProceedWithAction(SignatureVerificationAction action,
                         ReadErrorCallback callback);

  // Closes all the closable resources that the reader is using.
  void Close(base::OnceClosure callback);

  // This class internally transitions through the following states:
  //
  // kUninitialized -> kInitializing -> kInitialized -> kClosed
  //                         |
  //                         `--------> kError
  //
  // If initialization fails, the callback passed to `StartReading`
  // is called with the corresponding error, and the state changes to `kError`.
  // Recovery from an initialization error is not possible.
  enum class State {
    kUninitialized,
    kInitializing,
    kInitialized,
    kError,
    kClosed,
  };

  // This class is ready to read responses from the Signed Web Bundle iff its
  // state is `kInitialized`.
  State GetState() const { return state_; }

  SignedWebBundleReader(const SignedWebBundleReader&) = delete;
  SignedWebBundleReader& operator=(const SignedWebBundleReader&) = delete;

  ~SignedWebBundleReader();

  // Returns the integrity block of the Web Bundle.
  // Will CHECK if `GetState()` != `kInitialized`.
  const web_package::SignedWebBundleIntegrityBlock& GetIntegrityBlock() const;

  // Returns the primary URL, as specified in the metadata of the Web Bundle.
  // Will CHECK if `GetState()` != `kInitialized`.
  const std::optional<GURL>& GetPrimaryURL() const;

  // Returns the URLs of all exchanges contained in the Web Bundle, as specified
  // in the metadata. Will CHECK if `GetState()` != `kInitialized`.
  std::vector<GURL> GetEntries() const;

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
  // `content::WebBundleReader`) before matching it to a response. Will CHECK if
  // `GetState()` != `kInitialized`.
  using ResponseCallback = base::OnceCallback<void(
      base::expected<web_package::mojom::BundleResponsePtr,
                     ReadResponseError>)>;
  void ReadResponse(const network::ResourceRequest& resource_request,
                    ResponseCallback callback);

  // Reads the response body given a `response` read with `ReadResponse`. Will
  // CHECK if `GetState()` != `kInitialized`.
  using ResponseBodyCallback = base::OnceCallback<void(net::Error net_error)>;
  void ReadResponseBody(web_package::mojom::BundleResponsePtr response,
                        mojo::ScopedDataPipeProducerHandle producer_handle,
                        ResponseBodyCallback callback);

  base::WeakPtr<SignedWebBundleReader> AsWeakPtr();

 private:
  explicit SignedWebBundleReader(
      const base::FilePath& web_bundle_path,
      const std::optional<GURL>& base_url,
      std::unique_ptr<web_package::SignedWebBundleSignatureVerifier>
          signature_verifier);

  void OnFileOpened(
      IntegrityBlockReadResultCallback integrity_block_result_callback,
      base::File file);

  void OnIntegrityBlockParsed(
      IntegrityBlockReadResultCallback integrity_block_result_callback,
      web_package::mojom::BundleIntegrityBlockPtr integrity_block,
      web_package::mojom::BundleIntegrityBlockParseErrorPtr error);

  void OnFileLengthRead(
      ReadErrorCallback callback,
      base::expected<uint64_t, base::File::Error> file_length);

  void OnSignaturesVerified(
      const base::TimeTicks& verification_start_time,
      uint64_t file_length,
      ReadErrorCallback callback,
      base::expected<void, web_package::SignedWebBundleSignatureVerifier::Error>
          verification_result);

  void ReadMetadata(ReadErrorCallback callback);

  void OnMetadataParsed(ReadErrorCallback callback,
                        web_package::mojom::BundleMetadataPtr metadata,
                        web_package::mojom::BundleMetadataParseErrorPtr error);

  void FulfillWithError(ReadErrorCallback callback,
                        UnusableSwbnFileError error);

  void ReadResponseInternal(
      web_package::mojom::BundleResponseLocationPtr location,
      ResponseCallback callback);

  void OnResponseParsed(ResponseCallback callback,
                        web_package::mojom::BundleResponsePtr response,
                        web_package::mojom::BundleResponseParseErrorPtr error);
  void StartReadingFromDataSource(
      mojo::DataPipeProducer* data_pipe_producer,
      ResponseBodyCallback callback,
      std::unique_ptr<mojo::DataPipeProducer::DataSource> data_source);
  void OnResponseBodyRead(mojo::DataPipeProducer* producer,
                          ResponseBodyCallback callback,
                          MojoResult result);

  // The following method is a callback for reconnection handling if the
  // `SafeWebBundleParser` in the `SignedWebBundleParserConnection`
  // disconnects at some point after integrity block and
  // metadata have been read. Reconnecting to a new parser will be attempted on
  // the next call to `ReadResponse`.
  void OnReconnect(base::expected<void, std::string> status);

  void OnParserClosed(base::OnceClosure callback);
  void OnFileClosed(base::OnceClosure callback);
  void ReplyClosedIfNecessary();

  State state_ = State::kUninitialized;

  std::unique_ptr<web_package::SignedWebBundleSignatureVerifier>
      signature_verifier_;

  // Integrity Block
  std::optional<web_package::SignedWebBundleIntegrityBlock> integrity_block_;

  // Metadata
  std::optional<GURL> primary_url_;
  base::flat_map<GURL, web_package::mojom::BundleResponseLocationPtr> entries_;

  // Accumulates `ReadResponse` requests while the parser is disconnected, and
  // runs them after reconnection of the parser succeeds or fails.
  std::vector<std::pair<web_package::mojom::BundleResponseLocationPtr,
                        ResponseCallback>>
      pending_read_responses_;

  base::FilePath web_bundle_path_;
  std::optional<GURL> base_url_;
  std::unique_ptr<data_decoder::SafeWebBundleParser> parser_;
  base::flat_set<std::unique_ptr<mojo::DataPipeProducer>,
                 base::UniquePtrComparator>
      active_response_body_producers_;
  std::optional<base::File> file_;
  base::OnceClosure close_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SignedWebBundleReader> weak_ptr_factory_{this};
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
