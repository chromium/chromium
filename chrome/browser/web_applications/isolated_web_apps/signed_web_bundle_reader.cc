// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {

SignedWebBundleReader::SignedWebBundleReader(
    const base::FilePath& web_bundle_path,
    const absl::optional<GURL>& base_url,
    std::unique_ptr<web_package::SignedWebBundleSignatureVerifier>
        signature_verifier)
    : web_bundle_path_(web_bundle_path),
      base_url_(base_url),
      signature_verifier_(std::move(signature_verifier)) {}

SignedWebBundleReader::~SignedWebBundleReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<SignedWebBundleReader> SignedWebBundleReader::Create(
    const base::FilePath& web_bundle_path,
    const absl::optional<GURL>& base_url,
    std::unique_ptr<web_package::SignedWebBundleSignatureVerifier>
        signature_verifier) {
  return base::WrapUnique(new SignedWebBundleReader(
      web_bundle_path, base_url, std::move(signature_verifier)));
}

void SignedWebBundleReader::StartReading(
    IntegrityBlockReadResultCallback integrity_block_result_callback,
    ReadErrorCallback read_error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kUninitialized);

  Initialize(std::move(integrity_block_result_callback),
             std::move(read_error_callback));
}

void SignedWebBundleReader::Initialize(
    IntegrityBlockReadResultCallback integrity_block_result_callback,
    ReadErrorCallback read_error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kUninitialized);

  state_ = State::kInitializing;

  parser_ = std::make_unique<data_decoder::SafeWebBundleParser>(base_url_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](const base::FilePath& file_path) -> std::unique_ptr<base::File> {
            return std::make_unique<base::File>(
                file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
          },
          web_bundle_path_),
      base::BindOnce(&SignedWebBundleReader::OnFileOpened,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(integrity_block_result_callback),
                     std::move(read_error_callback)));
}

void SignedWebBundleReader::OnFileOpened(
    IntegrityBlockReadResultCallback integrity_block_result_callback,
    ReadErrorCallback read_error_callback,
    std::unique_ptr<base::File> file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  if (!file->IsValid()) {
    FulfillWithError(
        std::move(read_error_callback),
        web_package::mojom::BundleIntegrityBlockParseError::New(
            web_package::mojom::BundleParseErrorType::kParserInternalError,
            base::File::ErrorToString(file->error_details())));
    return;
  }

  file_ = base::MakeRefCounted<web_package::SharedFile>(std::move(file));
  file_->DuplicateFile(base::BindOnce(
      &SignedWebBundleReader::OnFileDuplicated, weak_ptr_factory_.GetWeakPtr(),
      std::move(integrity_block_result_callback),
      std::move(read_error_callback)));
}

void SignedWebBundleReader::OnFileDuplicated(
    IntegrityBlockReadResultCallback integrity_block_result_callback,
    ReadErrorCallback read_error_callback,
    base::File file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  base::File::Error error = parser_->OpenFile(std::move(file));
  if (error != base::File::FILE_OK) {
    FulfillWithError(
        std::move(read_error_callback),
        web_package::mojom::BundleIntegrityBlockParseError::New(
            web_package::mojom::BundleParseErrorType::kParserInternalError,
            base::File::ErrorToString(error)));
    return;
  }

  parser_->ParseIntegrityBlock(
      base::BindOnce(&SignedWebBundleReader::OnIntegrityBlockParsed,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(integrity_block_result_callback),
                     std::move(read_error_callback)));
}

void SignedWebBundleReader::OnIntegrityBlockParsed(
    IntegrityBlockReadResultCallback integrity_block_result_callback,
    ReadErrorCallback read_error_callback,
    web_package::mojom::BundleIntegrityBlockPtr raw_integrity_block,
    web_package::mojom::BundleIntegrityBlockParseErrorPtr error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  if (error) {
    FulfillWithError(std::move(read_error_callback), std::move(error));
    return;
  }

  auto integrity_block = web_package::SignedWebBundleIntegrityBlock::Create(
      std::move(raw_integrity_block));
  if (!integrity_block.has_value()) {
    FulfillWithError(
        std::move(read_error_callback),
        web_package::mojom::BundleIntegrityBlockParseError::New(
            web_package::mojom::BundleParseErrorType::kFormatError,
            base::StringPrintf("Error while parsing the Signed Web Bundle's "
                               "integrity block: %s",
                               integrity_block.error().c_str())));
    return;
  }

  integrity_block_size_in_bytes_ = integrity_block->size_in_bytes();

  std::move(integrity_block_result_callback)
      .Run(*integrity_block,
           base::BindOnce(&SignedWebBundleReader::
                              OnShouldContinueParsingAfterIntegrityBlock,
                          weak_ptr_factory_.GetWeakPtr(), *integrity_block,
                          std::move(read_error_callback)));
}

void SignedWebBundleReader::OnShouldContinueParsingAfterIntegrityBlock(
    web_package::SignedWebBundleIntegrityBlock integrity_block,
    ReadErrorCallback callback,
    SignatureVerificationAction action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  switch (action.type()) {
    case SignatureVerificationAction::Type::kAbort:
      FulfillWithError(std::move(callback),
                       AbortedByCaller({.message = action.abort_message()}));
      return;
    case SignatureVerificationAction::Type::kContinueAndVerifySignatures:
      VerifySignatures(std::move(integrity_block), std::move(callback));
      return;
    case SignatureVerificationAction::Type::
        kContinueAndSkipSignatureVerification:
      ReadMetadata(std::move(callback));
      return;
  }
}

void SignedWebBundleReader::VerifySignatures(
    web_package::SignedWebBundleIntegrityBlock integrity_block,
    ReadErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](scoped_refptr<web_package::SharedFile> file)
              -> base::expected<uint64_t, base::File::Error> {
            int64_t length = (*file)->GetLength();
            if (length < 0) {
              return base::unexpected((*file)->GetLastFileError());
            }
            return static_cast<uint64_t>(length);
          },
          file_),
      base::BindOnce(&SignedWebBundleReader::OnFileLengthRead,
                     weak_ptr_factory_.GetWeakPtr(), std::move(integrity_block),
                     std::move(callback)));
}

void SignedWebBundleReader::OnFileLengthRead(
    web_package::SignedWebBundleIntegrityBlock integrity_block,
    ReadErrorCallback callback,
    base::expected<uint64_t, base::File::Error> file_length) {
  if (!file_length.has_value()) {
    FulfillWithError(
        std::move(callback),
        web_package::mojom::BundleIntegrityBlockParseError::New(
            web_package::mojom::BundleParseErrorType::kParserInternalError,
            base::File::ErrorToString(file_length.error())));
    return;
  }

  signature_verifier_->VerifySignatures(
      file_, std::move(integrity_block),
      base::BindOnce(&SignedWebBundleReader::OnSignaturesVerified,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     *file_length, std::move(callback)));
}

void SignedWebBundleReader::OnSignaturesVerified(
    const base::TimeTicks& verification_start_time,
    uint64_t file_length,
    ReadErrorCallback callback,
    absl::optional<web_package::SignedWebBundleSignatureVerifier::Error>
        verification_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  base::UmaHistogramMediumTimes(
      "WebApp.Isolated.SignatureVerificationDuration",
      base::TimeTicks::Now() - verification_start_time);
  // Measure file length in MiB up to ~10GiB.
  base::UmaHistogramCounts10000(
      "WebApp.Isolated.SignatureVerificationFileLength",
      base::saturated_cast<int>(std::round(file_length / (1024.0 * 1024.0))));

  if (verification_error.has_value()) {
    FulfillWithError(std::move(callback), *verification_error);
    return;
  }

  // Signatures are valid; continue with parsing of metadata.
  ReadMetadata(std::move(callback));
}

void SignedWebBundleReader::ReadMetadata(ReadErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  parser_->ParseMetadata(
      /*offset=*/base::checked_cast<int64_t>(*integrity_block_size_in_bytes_),
      base::BindOnce(&SignedWebBundleReader::OnMetadataParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SignedWebBundleReader::OnMetadataParsed(
    ReadErrorCallback callback,
    web_package::mojom::BundleMetadataPtr metadata,
    web_package::mojom::BundleMetadataParseErrorPtr error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  if (error) {
    FulfillWithError(std::move(callback), std::move(error));
    return;
  }

  primary_url_ = metadata->primary_url;
  entries_ = std::move(metadata->requests);

  state_ = State::kInitialized;

  parser_->SetDisconnectCallback(
      base::BindOnce(&SignedWebBundleReader::OnParserDisconnected,
                     // `base::Unretained` is okay to use here, since
                     // `parser_` will be deleted before `this` is deleted.
                     base::Unretained(this)));

  std::move(callback).Run(absl::nullopt);
}

void SignedWebBundleReader::FulfillWithError(
    ReadErrorCallback callback,
    ReadIntegrityBlockAndMetadataError error) {
  state_ = State::kError;

  // This is an irrecoverable error state, thus we can safely delete `parser_`
  // here to free up resources.
  parser_.reset();

  std::move(callback).Run(std::move(error));
}

const absl::optional<GURL>& SignedWebBundleReader::GetPrimaryURL() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitialized);

  return primary_url_;
}

std::vector<GURL> SignedWebBundleReader::GetEntries() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitialized);

  std::vector<GURL> entries;
  entries.reserve(entries_.size());
  base::ranges::transform(entries_, std::back_inserter(entries),
                          [](const auto& entry) { return entry.first; });
  return entries;
}

void SignedWebBundleReader::ReadResponse(
    const network::ResourceRequest& resource_request,
    ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitialized);

  const GURL& url = net::SimplifyUrlForRequest(resource_request.url);
  auto entry_it = entries_.find(url);
  if (entry_it == entries_.end()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(ReadResponseError::ForResponseNotFound(
                base::StringPrintf("The Web Bundle does not contain a response "
                                   "for the provided URL: %s",
                                   url.spec().c_str())))));
    return;
  }

  auto response_location = entry_it->second->Clone();
  if (is_disconnected_) {
    // Try reconnecting the parser if it hasn't been attempted yet.
    if (pending_read_responses_.empty()) {
      Reconnect();
    }
    pending_read_responses_.emplace_back(std::move(response_location),
                                         std::move(callback));
    return;
  }

  ReadResponseInternal(std::move(response_location), std::move(callback));
}

void SignedWebBundleReader::ReadResponseInternal(
    web_package::mojom::BundleResponseLocationPtr location,
    ResponseCallback callback) {
  CHECK_EQ(state_, State::kInitialized);

  parser_->ParseResponse(
      location->offset, location->length,
      base::BindOnce(&SignedWebBundleReader::OnResponseParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SignedWebBundleReader::OnResponseParsed(
    ResponseCallback callback,
    web_package::mojom::BundleResponsePtr response,
    web_package::mojom::BundleResponseParseErrorPtr error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitialized);

  if (error) {
    std::move(callback).Run(base::unexpected(
        ReadResponseError::FromBundleParseError(std::move(error))));
  } else {
    std::move(callback).Run(std::move(response));
  }
}

void SignedWebBundleReader::ReadResponseBody(
    web_package::mojom::BundleResponsePtr response,
    mojo::ScopedDataPipeProducerHandle producer_handle,
    ResponseBodyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitialized);

  auto data_producer =
      std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
  mojo::DataPipeProducer* raw_producer = data_producer.get();
  raw_producer->Write(
      file_->CreateDataSource(response->payload_offset,
                              response->payload_length),
      base::BindOnce(
          // `producer` is passed to this callback purely for its lifetime
          // management so that it is deleted once this callback runs.
          [](std::unique_ptr<mojo::DataPipeProducer> producer,
             MojoResult result) -> net::Error {
            return result == MOJO_RESULT_OK ? net::Error::OK
                                            : net::Error::ERR_UNEXPECTED;
          },
          std::move(data_producer))
          .Then(std::move(callback)));
}

base::WeakPtr<SignedWebBundleReader> SignedWebBundleReader::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SignedWebBundleReader::SetParserDisconnectCallbackForTesting(
    base::RepeatingClosure callback) {
  parser_disconnect_callback_for_testing_ = std::move(callback);
}

void SignedWebBundleReader::SetReconnectionFileErrorForTesting(
    base::File::Error file_error) {
  reconnection_file_error_for_testing_ = file_error;
}

void SignedWebBundleReader::OnParserDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_disconnected_);

  is_disconnected_ = true;
  parser_ = nullptr;
  if (!parser_disconnect_callback_for_testing_.is_null()) {
    CHECK_IS_TEST();
    parser_disconnect_callback_for_testing_.Run();
  }
  // Reconnection will be attempted on the next call to `ReadResponse`.
}

void SignedWebBundleReader::Reconnect() {
  DCHECK(!parser_);
  parser_ = std::make_unique<data_decoder::SafeWebBundleParser>(base_url_);

  file_->DuplicateFile(base::BindOnce(&SignedWebBundleReader::ReconnectForFile,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void SignedWebBundleReader::ReconnectForFile(base::File file) {
  base::File::Error file_error;
  if (reconnection_file_error_for_testing_.has_value()) {
    CHECK_IS_TEST();
    file_error = *reconnection_file_error_for_testing_;
  } else {
    file_error = parser_->OpenFile(std::move(file));
  }

  absl::optional<std::string> error;
  if (file_error != base::File::FILE_OK) {
    error = base::File::ErrorToString(file_error);
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SignedWebBundleReader::DidReconnect,
                     weak_ptr_factory_.GetWeakPtr(), std::move(error)));
}

void SignedWebBundleReader::DidReconnect(absl::optional<std::string> error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_disconnected_);
  DCHECK(parser_);
  std::vector<std::pair<web_package::mojom::BundleResponseLocationPtr,
                        ResponseCallback>>
      read_tasks;
  read_tasks.swap(pending_read_responses_);

  if (error) {
    for (auto& [response_location, response_callback] : read_tasks) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(response_callback),
              base::unexpected(
                  ReadResponseError::ForParserInternalError(base::StringPrintf(
                      "Unable to open file: %s", error->c_str())))));
    }
    return;
  }

  is_disconnected_ = false;
  parser_->SetDisconnectCallback(
      base::BindOnce(&SignedWebBundleReader::OnParserDisconnected,
                     // `base::Unretained` is okay to use here, since `parser_`
                     // will be deleted before `this` is deleted.
                     base::Unretained(this)));
  for (auto& [response_location, response_callback] : read_tasks) {
    ReadResponseInternal(std::move(response_location),
                         std::move(response_callback));
  }
}

// static
SignedWebBundleReader::ReadResponseError
SignedWebBundleReader::ReadResponseError::FromBundleParseError(
    web_package::mojom::BundleResponseParseErrorPtr error) {
  switch (error->type) {
    case web_package::mojom::BundleParseErrorType::kVersionError:
      // A `kVersionError` error can only be triggered while parsing
      // the integrity block or metadata, not while parsing a response.
      NOTREACHED();
      [[fallthrough]];
    case web_package::mojom::BundleParseErrorType::kParserInternalError:
      return ForParserInternalError(error->message);
    case web_package::mojom::BundleParseErrorType::kFormatError:
      return ReadResponseError(Type::kFormatError, error->message);
  }
}

// static
SignedWebBundleReader::ReadResponseError
SignedWebBundleReader::ReadResponseError::ForParserInternalError(
    const std::string& message) {
  return ReadResponseError(Type::kParserInternalError, message);
}

// static
SignedWebBundleReader::ReadResponseError
SignedWebBundleReader::ReadResponseError::ForResponseNotFound(
    const std::string& message) {
  return ReadResponseError(Type::kResponseNotFound, message);
}

// static
SignedWebBundleReader::SignatureVerificationAction
SignedWebBundleReader::SignatureVerificationAction::Abort(
    const std::string& abort_message) {
  return SignatureVerificationAction(Type::kAbort, abort_message);
}

// static
SignedWebBundleReader::SignatureVerificationAction SignedWebBundleReader::
    SignatureVerificationAction::ContinueAndVerifySignatures() {
  return SignatureVerificationAction(Type::kContinueAndVerifySignatures,
                                     absl::nullopt);
}

// static
SignedWebBundleReader::SignatureVerificationAction SignedWebBundleReader::
    SignatureVerificationAction::ContinueAndSkipSignatureVerification() {
  return SignatureVerificationAction(
      Type::kContinueAndSkipSignatureVerification, absl::nullopt);
}

SignedWebBundleReader::SignatureVerificationAction::SignatureVerificationAction(
    Type type,
    absl::optional<std::string> abort_message)
    : type_(type), abort_message_(abort_message) {}

SignedWebBundleReader::SignatureVerificationAction::SignatureVerificationAction(
    const SignatureVerificationAction&) = default;

SignedWebBundleReader::SignatureVerificationAction::
    ~SignatureVerificationAction() = default;

}  // namespace web_app
