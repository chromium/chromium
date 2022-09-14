// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_integrity_block.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace web_app {

SignedWebBundleReader::SignedWebBundleReader(
    const base::FilePath& web_bundle_path)
    : web_bundle_path_(web_bundle_path) {}

SignedWebBundleReader::~SignedWebBundleReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<SignedWebBundleReader>
SignedWebBundleReader::CreateAndStartReading(
    const base::FilePath& web_bundle_path,
    IntegrityBlockReadResultCallback integrity_block_result_callback,
    ReadErrorCallback read_error_callback) {
  // Using `new` to access a non-public constructor.
  auto reader = base::WrapUnique(new SignedWebBundleReader(web_bundle_path));
  reader->Initialize(std::move(integrity_block_result_callback),
                     std::move(read_error_callback));

  return reader;
}

void SignedWebBundleReader::Initialize(
    IntegrityBlockReadResultCallback integrity_block_result_callback,
    ReadErrorCallback read_error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  parser_ = std::make_unique<data_decoder::SafeWebBundleParser>();

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

  auto integrity_block =
      SignedWebBundleIntegrityBlock::Create(std::move(raw_integrity_block));
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
  integrity_block_ = std::move(*integrity_block);

  std::move(integrity_block_result_callback)
      .Run(integrity_block_->GetPublicKeyStack(),
           base::BindOnce(&SignedWebBundleReader::
                              OnShouldContinueParsingAfterIntegrityBlock,
                          weak_ptr_factory_.GetWeakPtr(),
                          std::move(read_error_callback)));
}

void SignedWebBundleReader::OnShouldContinueParsingAfterIntegrityBlock(
    ReadErrorCallback callback,
    IntegrityVerificationAction action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  switch (action.type()) {
    case IntegrityVerificationAction::Type::kAbort:
      FulfillWithError(std::move(callback),
                       AbortedByCaller({.message = action.abort_message()}));
      return;
    case IntegrityVerificationAction::Type::kContinueAndVerifyIntegrity:
      VerifyIntegrity(std::move(callback));
      return;
#if BUILDFLAG(IS_CHROMEOS)
    case IntegrityVerificationAction::Type::
        kContinueAndSkipIntegrityVerification:
      ReadMetadata(std::move(callback));
      return;
#endif
  }
}

void SignedWebBundleReader::VerifyIntegrity(ReadErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  // TODO(crbug.com/1315947): Actually verify integrity here.
  OnIntegrityVerified(std::move(callback));
}

void SignedWebBundleReader::OnIntegrityVerified(ReadErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  // TODO(crbug.com/1315947): Check whether the integrity has been verified
  // successfully here, once it is implemented.

  ReadMetadata(std::move(callback));
}

void SignedWebBundleReader::ReadMetadata(ReadErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  parser_->ParseMetadata(
      /*offset=*/base::checked_cast<int64_t>(integrity_block_->size_in_bytes()),
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

void SignedWebBundleReader::FulfillWithError(ReadErrorCallback callback,
                                             ReadError error) {
  state_ = State::kError;

  // This is an irrecoverable error state, thus we can safely delete `parser_`
  // here to free up resources. We do so asynchronously, since this method might
  // be called in response to `SafeWebBundleParser::OnDisconnect` if the parser
  // disconnects while parsing the integrity block or metadata. Deleting
  // `parser_` synchronously here might cause a use after free if `callback`
  // deletes `this` in response to the error, because `parser_` would attempt to
  // access its already freed instance variables when its `OnDisconnect` method
  // continues execution after running this callback.
  base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                     std::move(parser_));

  std::move(callback).Run(std::move(error));
}

GURL SignedWebBundleReader::GetPrimaryURL() const {
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
    base::SequencedTaskRunnerHandle::Get()->PostTask(
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
    if (pending_read_responses_.empty())
      Reconnect();
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
  parser_ = std::make_unique<data_decoder::SafeWebBundleParser>();

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
  if (file_error != base::File::FILE_OK)
    error = base::File::ErrorToString(file_error);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
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
      base::SequencedTaskRunnerHandle::Get()->PostTask(
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
SignedWebBundleReader::IntegrityVerificationAction
SignedWebBundleReader::IntegrityVerificationAction::Abort(
    const std::string& abort_message) {
  return IntegrityVerificationAction(Type::kAbort, abort_message);
}

// static
SignedWebBundleReader::IntegrityVerificationAction SignedWebBundleReader::
    IntegrityVerificationAction::ContinueAndVerifyIntegrity() {
  return IntegrityVerificationAction(Type::kContinueAndVerifyIntegrity,
                                     absl::nullopt);
}

#if BUILDFLAG(IS_CHROMEOS)

// static
SignedWebBundleReader::IntegrityVerificationAction SignedWebBundleReader::
    IntegrityVerificationAction::ContinueAndSkipIntegrityVerification() {
  return IntegrityVerificationAction(
      Type::kContinueAndSkipIntegrityVerification, absl::nullopt);
}

#endif

SignedWebBundleReader::IntegrityVerificationAction::IntegrityVerificationAction(
    Type type,
    absl::optional<std::string> abort_message)
    : type_(type), abort_message_(abort_message) {}

SignedWebBundleReader::IntegrityVerificationAction::IntegrityVerificationAction(
    const IntegrityVerificationAction&) = default;

SignedWebBundleReader::IntegrityVerificationAction::
    ~IntegrityVerificationAction() = default;

}  // namespace web_app
