// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/containers/to_vector.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/file_data_source.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace web_app {
namespace {
// This is blocking operation.
base::expected<uint64_t, base::File::Error> ReadLengthOfFile(base::File file) {
  int64_t length = file.GetLength();
  if (length < 0) {
    return base::unexpected(file.GetLastFileError());
  }
  return static_cast<uint64_t>(length);
}

void OpenFile(const base::FilePath& file_path,
              base::OnceCallback<void(base::File)> open_callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](const base::FilePath& file_path) {
            return base::File(file_path,
                              base::File::FLAG_OPEN | base::File::FLAG_READ);
          },
          file_path),
      std::move(open_callback));
}

void CloseFile(base::File file, base::OnceClosure close_callback) {
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce([](base::File file) { file.Close(); }, std::move(file)),
      std::move(close_callback));
}

void OpenFileDataSource(
    base::File file,
    uint64_t start,
    uint64_t end,
    base::OnceCallback<void(
        std::unique_ptr<mojo::DataPipeProducer::DataSource>)> open_callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](base::File file, uint64_t start, uint64_t end) {
            auto file_data_source =
                std::make_unique<mojo::FileDataSource>(std::move(file));
            file_data_source->SetRange(start, end);
            return file_data_source;
          },
          std::move(file), start, end),
      std::move(open_callback));
}

}  // namespace

SignedWebBundleReader::SignedWebBundleReader(
    const base::FilePath& web_bundle_path,
    const std::optional<GURL>& base_url,
    std::unique_ptr<web_package::SignedWebBundleSignatureVerifier>
        signature_verifier)
    : signature_verifier_(std::move(signature_verifier)),
      web_bundle_path_(web_bundle_path),
      base_url_(base_url) {}

SignedWebBundleReader::~SignedWebBundleReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (file_.has_value()) {
    CloseFile(std::move(*file_), base::DoNothing());
  }
}

// static
std::unique_ptr<SignedWebBundleReader> SignedWebBundleReader::Create(
    const base::FilePath& web_bundle_path,
    const std::optional<GURL>& base_url,
    std::unique_ptr<web_package::SignedWebBundleSignatureVerifier>
        signature_verifier) {
  return base::WrapUnique(new SignedWebBundleReader(
      web_bundle_path, base_url, std::move(signature_verifier)));
}

void SignedWebBundleReader::Close(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(state_ == State::kInitialized || state_ == State::kError);
  if (state_ != State::kError) {
    state_ = State::kClosed;
  }
  CloseFile(
      std::move(*file_),
      base::BindOnce(&SignedWebBundleReader::OnFileClosed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SignedWebBundleReader::OnFileClosed(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(state_ == State::kClosed || state_ == State::kError)
      << base::to_underlying(state_);

  parser_->Close(base::BindOnce(&SignedWebBundleReader::OnParserClosed,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(callback)));
}

void SignedWebBundleReader::OnParserClosed(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(state_ == State::kClosed || state_ == State::kError)
      << base::to_underlying(state_);
  close_callback_ = std::move(callback);
  ReplyClosedIfNecessary();
}

void SignedWebBundleReader::ReadIntegrityBlock(
    IntegrityBlockReadResultCallback integrity_block_result_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kUninitialized);

  state_ = State::kInitializing;
  OpenFile(web_bundle_path_,
           base::BindOnce(&SignedWebBundleReader::OnFileOpened,
                          weak_ptr_factory_.GetWeakPtr(),
                          std::move(integrity_block_result_callback)));
}

void SignedWebBundleReader::OnFileOpened(
    IntegrityBlockReadResultCallback integrity_block_result_callback,
    base::File file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  parser_ = std::make_unique<data_decoder::SafeWebBundleParser>(
      base_url_,
      data_decoder::SafeWebBundleParser::GetFileStrategy(file.Duplicate()));

  file_ = std::move(file);

  parser_->ParseIntegrityBlock(
      base::BindOnce(&SignedWebBundleReader::OnIntegrityBlockParsed,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(integrity_block_result_callback)));
}

void SignedWebBundleReader::OnIntegrityBlockParsed(
    IntegrityBlockReadResultCallback integrity_block_result_callback,
    web_package::mojom::BundleIntegrityBlockPtr raw_integrity_block,
    web_package::mojom::BundleIntegrityBlockParseErrorPtr error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  if (error) {
    std::move(integrity_block_result_callback)
        .Run(base::unexpected(UnusableSwbnFileError(std::move(error))));
    return;
  }

  auto integrity_block =
      web_package::SignedWebBundleIntegrityBlock::Create(
          std::move(raw_integrity_block))
          .transform_error([&](std::string error) {
            return UnusableSwbnFileError(
                UnusableSwbnFileError::Error::kIntegrityBlockParserFormatError,
                "Error while parsing the Signed Web Bundle's integrity "
                "block: " +
                    std::move(error));
          });

  if (integrity_block.has_value()) {
    integrity_block_ = integrity_block.value();
  }
  std::move(integrity_block_result_callback).Run(integrity_block);
}

void SignedWebBundleReader::ProceedWithAction(
    SignatureVerificationAction action,
    ReadErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  switch (action.type()) {
    case SignatureVerificationAction::Type::kAbort:
      FulfillWithError(std::move(callback), std::move(action).error());
      return;
    case SignatureVerificationAction::Type::kContinueAndVerifySignatures:
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(ReadLengthOfFile, file_->Duplicate()),
          base::BindOnce(&SignedWebBundleReader::OnFileLengthRead,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
      return;
    case SignatureVerificationAction::Type::
        kContinueAndSkipSignatureVerification:
      ReadMetadata(std::move(callback));
      return;
  }
}

void SignedWebBundleReader::OnFileLengthRead(
    ReadErrorCallback callback,
    base::expected<uint64_t, base::File::Error> file_length) {
  RETURN_IF_ERROR(file_length, [&](base::File::Error error) {
    FulfillWithError(
        std::move(callback),
        UnusableSwbnFileError(
            UnusableSwbnFileError::Error::kIntegrityBlockParserInternalError,
            base::File::ErrorToString(error)));
  });

  CHECK(integrity_block_.has_value())
      << "The integrity block must have been read before verifying signatures.";
  signature_verifier_->VerifySignatures(
      file_->Duplicate(), *integrity_block_,
      base::BindOnce(&SignedWebBundleReader::OnSignaturesVerified,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     *file_length, std::move(callback)));
}

void SignedWebBundleReader::OnSignaturesVerified(
    const base::TimeTicks& verification_start_time,
    uint64_t file_length,
    ReadErrorCallback callback,
    base::expected<void, web_package::SignedWebBundleSignatureVerifier::Error>
        verification_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  base::UmaHistogramMediumTimes(
      "WebApp.Isolated.SignatureVerificationDuration",
      base::TimeTicks::Now() - verification_start_time);
  // Measure file length in MiB up to ~10GiB.
  base::UmaHistogramCounts10000(
      "WebApp.Isolated.SignatureVerificationFileLength",
      base::saturated_cast<int>(std::round(file_length / (1024.0 * 1024.0))));

  RETURN_IF_ERROR(
      verification_result,
      [&](web_package::SignedWebBundleSignatureVerifier::Error error) {
        FulfillWithError(std::move(callback), UnusableSwbnFileError(error));
      });

  // Signatures are valid; continue with parsing of metadata.
  ReadMetadata(std::move(callback));
}

void SignedWebBundleReader::ReadMetadata(ReadErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  CHECK(integrity_block_.has_value())
      << "The integrity block must have been read before reading metadata.";
  uint64_t metadata_offset = integrity_block_->size_in_bytes();

  parser_->ParseMetadata(
      metadata_offset,
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
    FulfillWithError(std::move(callback), UnusableSwbnFileError(error));
    return;
  }

  primary_url_ = metadata->primary_url;
  entries_ = std::move(metadata->requests);

  state_ = State::kInitialized;

  std::move(callback).Run(base::ok());
}

void SignedWebBundleReader::FulfillWithError(ReadErrorCallback callback,
                                             UnusableSwbnFileError error) {
  state_ = State::kError;
  Close(
      base::BindOnce(std::move(callback), base::unexpected(std::move(error))));
}

const web_package::SignedWebBundleIntegrityBlock&
SignedWebBundleReader::GetIntegrityBlock() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitialized);

  return *integrity_block_;
}

const std::optional<GURL>& SignedWebBundleReader::GetPrimaryURL() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitialized);

  return primary_url_;
}

std::vector<GURL> SignedWebBundleReader::GetEntries() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitialized);

  return base::ToVector(entries_,
                        [](const auto& entry) { return entry.first; });
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
        base::BindOnce(std::move(callback),
                       base::unexpected(ReadResponseError::ForResponseNotFound(
                           "The Web Bundle does not contain a response for the "
                           "provided URL: " +
                           url.spec()))));
    return;
  }

  ReadResponseInternal(entry_it->second->Clone(), std::move(callback));
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

  uint64_t response_start = response->payload_offset;
  uint64_t response_end;
  if (!base::CheckAdd(response_start, response->payload_length)
           .AssignIfValid(&response_end)) {
    // Response end doesn't fit in uint64_t.
    OnResponseBodyRead(nullptr, std::move(callback),
                       MOJO_RESULT_INVALID_ARGUMENT);
    return;
  }

  auto data_producer =
      std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
  mojo::DataPipeProducer* raw_producer = data_producer.get();
  active_response_body_producers_.insert(std::move(data_producer));

  OpenFileDataSource(
      file_->Duplicate(), response_start, response_end,
      base::BindOnce(&SignedWebBundleReader::StartReadingFromDataSource,
                     this->AsWeakPtr(), raw_producer, std::move(callback)));
}

void SignedWebBundleReader::StartReadingFromDataSource(
    mojo::DataPipeProducer* data_pipe_producer,
    ResponseBodyCallback callback,
    std::unique_ptr<mojo::DataPipeProducer::DataSource> data_source) {
  data_pipe_producer->Write(
      std::move(data_source),
      base::BindOnce(&SignedWebBundleReader::OnResponseBodyRead,
                     this->AsWeakPtr(), data_pipe_producer,
                     std::move(callback)));
}

void SignedWebBundleReader::OnResponseBodyRead(mojo::DataPipeProducer* producer,
                                               ResponseBodyCallback callback,
                                               MojoResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  active_response_body_producers_.erase(producer);
  net::Error net_result =
      result == MOJO_RESULT_OK ? net::Error::OK : net::Error::ERR_UNEXPECTED;
  std::move(callback).Run(net_result);
  ReplyClosedIfNecessary();
}

void SignedWebBundleReader::ReplyClosedIfNecessary() {
  if (active_response_body_producers_.empty() && !close_callback_.is_null()) {
    std::move(close_callback_).Run();
  }
}

base::WeakPtr<SignedWebBundleReader> SignedWebBundleReader::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// static
SignedWebBundleReader::ReadResponseError
SignedWebBundleReader::ReadResponseError::FromBundleParseError(
    web_package::mojom::BundleResponseParseErrorPtr error) {
  switch (error->type) {
    case web_package::mojom::BundleParseErrorType::kVersionError:
      // A `kVersionError` error can only be triggered while parsing
      // the integrity block or metadata, not while parsing a response.
      NOTREACHED_IN_MIGRATION();
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
    UnusableSwbnFileError error) {
  return SignatureVerificationAction(Type::kAbort, std::move(error));
}

// static
SignedWebBundleReader::SignatureVerificationAction SignedWebBundleReader::
    SignatureVerificationAction::ContinueAndVerifySignatures() {
  return SignatureVerificationAction(Type::kContinueAndVerifySignatures,
                                     std::nullopt);
}

// static
SignedWebBundleReader::SignatureVerificationAction SignedWebBundleReader::
    SignatureVerificationAction::ContinueAndSkipSignatureVerification() {
  return SignatureVerificationAction(
      Type::kContinueAndSkipSignatureVerification, std::nullopt);
}

SignedWebBundleReader::SignatureVerificationAction::SignatureVerificationAction(
    Type type,
    std::optional<UnusableSwbnFileError> error)
    : type_(type), error_(std::move(error)) {}

SignedWebBundleReader::SignatureVerificationAction::SignatureVerificationAction(
    const SignatureVerificationAction&) = default;

SignedWebBundleReader::SignatureVerificationAction::
    ~SignatureVerificationAction() = default;

UnsecureReader::UnsecureReader(const base::FilePath& web_bundle_path)
    : web_bundle_path_(web_bundle_path) {}

UnsecureReader::~UnsecureReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UnsecureReader::StartReading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OpenFile(web_bundle_path_,
           base::BindOnce(&UnsecureReader::OnFileOpened, GetWeakPtr()));
}

void UnsecureReader::OnFileOpened(base::File file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  parser_ = std::make_unique<data_decoder::SafeWebBundleParser>(
      /*base_url=*/std::nullopt,
      data_decoder::SafeWebBundleParser::GetFileStrategy(std::move(file)));

  DoReading();
}

// static
void UnsecureSignedWebBundleIdReader::GetWebBundleId(
    const base::FilePath& web_bundle_path,
    WebBundleIdCallback result_callback) {
  std::unique_ptr<UnsecureSignedWebBundleIdReader> reader =
      base::WrapUnique(new UnsecureSignedWebBundleIdReader(web_bundle_path));
  UnsecureSignedWebBundleIdReader* const reader_raw_ptr = reader.get();

  // We pass the owning unique_ptr to the second no-op callback to keep
  // the instance of UnsecureSignedWebBundleIdReader alive.
  WebBundleIdCallback id_read_callback =
      base::BindOnce(std::move(result_callback))
          .Then(base::BindOnce(
              [](std::unique_ptr<UnsecureSignedWebBundleIdReader> owning_ptr) {
              },
              std::move(reader)));

  reader_raw_ptr->SetResultCallback(std::move(id_read_callback));
  reader_raw_ptr->StartReading();
}

UnsecureSignedWebBundleIdReader::UnsecureSignedWebBundleIdReader(
    const base::FilePath& web_bundle_path)
    : UnsecureReader(web_bundle_path) {}

void UnsecureSignedWebBundleIdReader::DoReading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  parser_->ParseIntegrityBlock(
      base::BindOnce(&UnsecureSignedWebBundleIdReader::OnIntegrityBlockParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UnsecureSignedWebBundleIdReader::ReturnError(UnusableSwbnFileError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(web_bundle_id_callback_).Run(base::unexpected(std::move(error)));
}

base::WeakPtr<UnsecureReader> UnsecureSignedWebBundleIdReader::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void UnsecureSignedWebBundleIdReader::OnIntegrityBlockParsed(
    web_package::mojom::BundleIntegrityBlockPtr raw_integrity_block,
    web_package::mojom::BundleIntegrityBlockParseErrorPtr error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    ReturnError(UnusableSwbnFileError(std::move(error)));
    return;
  }

  auto integrity_block =
      web_package::SignedWebBundleIntegrityBlock::Create(
          std::move(raw_integrity_block))
          .transform_error([](std::string error) {
            return UnusableSwbnFileError(
                UnusableSwbnFileError::Error::kIntegrityBlockParserFormatError,
                "Error while parsing the Signed Web Bundle's integrity "
                "block: " +
                    std::move(error));
          });

  if (!integrity_block.has_value()) {
    ReturnError(std::move(integrity_block.error()));
    return;
  }

  std::move(web_bundle_id_callback_).Run(integrity_block->web_bundle_id());
}

void UnsecureSignedWebBundleIdReader::SetResultCallback(
    WebBundleIdCallback web_bundle_id_result_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  web_bundle_id_callback_ = std::move(web_bundle_id_result_callback);
}

UnsecureSignedWebBundleIdReader::~UnsecureSignedWebBundleIdReader() = default;

}  // namespace web_app
