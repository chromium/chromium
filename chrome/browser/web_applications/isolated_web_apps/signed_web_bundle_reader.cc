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

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/state_transitions.h"
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

web_package::SignedWebBundleSignatureVerifier*
    g_signature_verifier_for_testing = nullptr;

class SignedWebBundleReaderImpl : public SignedWebBundleReader {
 public:
  using Callback =
      base::OnceCallback<void(base::expected<void, UnusableSwbnFileError>)>;

  SignedWebBundleReaderImpl(base::PassKey<SignedWebBundleReader>,
                            const base::FilePath& web_bundle_path,
                            const std::optional<GURL>& base_url,
                            bool verify_signatures)
      : web_bundle_path_(web_bundle_path),
        base_url_(base_url),
        verify_signatures_(verify_signatures) {}

  SignedWebBundleReaderImpl(const SignedWebBundleReaderImpl&) = delete;
  SignedWebBundleReaderImpl& operator=(const SignedWebBundleReaderImpl&) =
      delete;

  ~SignedWebBundleReaderImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (file_.has_value()) {
      CloseFile(std::move(*file_), base::DoNothing());
    }
  }

  void Start(Callback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_EQ(state_, State::kUninitialized);
    callback_ = std::move(callback);

    SetState(State::kInitializing);
    OpenFile(web_bundle_path_,
             base::BindOnce(&SignedWebBundleReaderImpl::OnFileOpened,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void Close(base::OnceClosure callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(state_ == State::kInitialized || state_ == State::kError);
    if (state_ != State::kError) {
      SetState(State::kClosed);
    }
    CloseFile(
        std::move(*file_),
        base::BindOnce(&SignedWebBundleReaderImpl::OnFileClosed,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  bool IsClosed() const override {
    CHECK_NE(state_, State::kError);
    return state_ == State::kClosed;
  }

  bool IsReady() const { return state_ == State::kInitialized; }

  const web_package::SignedWebBundleIntegrityBlock& GetIntegrityBlock()
      const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_EQ(state_, State::kInitialized);

    return *integrity_block_;
  }

  const std::optional<GURL>& GetPrimaryURL() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_EQ(state_, State::kInitialized);

    return primary_url_;
  }

  std::vector<GURL> GetEntries() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_EQ(state_, State::kInitialized);

    return base::ToVector(entries_,
                          [](const auto& entry) { return entry.first; });
  }

  void ReadResponse(const network::ResourceRequest& resource_request,
                    ResponseCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_EQ(state_, State::kInitialized);

    const GURL& url = net::SimplifyUrlForRequest(resource_request.url);
    auto* entry = base::FindOrNull(entries_, url);
    if (!entry) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(callback),
              base::unexpected(ReadResponseError::ForResponseNotFound(
                  "The Web Bundle does not contain a response for the "
                  "provided URL: " +
                  url.spec()))));
      return;
    }

    ReadResponseInternal(entry->Clone(), std::move(callback));
  }

  void ReadResponseBody(web_package::mojom::BundleResponsePtr response,
                        mojo::ScopedDataPipeProducerHandle producer_handle,
                        ResponseBodyCallback callback) override {
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
        base::BindOnce(&SignedWebBundleReaderImpl::StartReadingFromDataSource,
                       weak_ptr_factory_.GetWeakPtr(), raw_producer,
                       std::move(callback)));
  }

  base::WeakPtr<SignedWebBundleReader> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
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

#if DCHECK_IS_ON()
  friend base::StateTransitions<State>;
  friend std::ostream& operator<<(std::ostream& os, State state) {
    return os << static_cast<uint32_t>(state);
  }
#endif  // DCHECK_IS_ON()

  void SetState(State state) {
#if DCHECK_IS_ON()
    // See clang-format off
    static const base::NoDestructor<base::StateTransitions<State>> kTransitions(
        base::StateTransitions<State>({
            {State::kUninitialized, {State::kInitializing}},
            {State::kInitializing, {State::kInitialized, State::kError}},
            {State::kInitialized, {State::kClosed}},
            {State::kError, {}},
            {State::kClosed, {}},
        }));
    // clang-format on
    DCHECK_STATE_TRANSITION(kTransitions, state_, state);
#endif  // DCHECK_IS_ON()
    state_ = state;
  }

  void OnFileClosed(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(state_ == State::kClosed || state_ == State::kError)
        << base::to_underlying(state_);

    parser_->Close(base::BindOnce(&SignedWebBundleReaderImpl::OnParserClosed,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(callback)));
  }

  void OnParserClosed(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(state_ == State::kClosed || state_ == State::kError)
        << base::to_underlying(state_);
    close_callback_ = std::move(callback);
    ReplyClosedIfNecessary();
  }

  void OnFileOpened(base::File file) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_EQ(state_, State::kInitializing);

    parser_ = std::make_unique<data_decoder::SafeWebBundleParser>(
        base_url_,
        data_decoder::SafeWebBundleParser::GetFileStrategy(file.Duplicate()));

    file_ = std::move(file);

    parser_->ParseIntegrityBlock(
        base::BindOnce(&SignedWebBundleReaderImpl::OnIntegrityBlockParsed,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnIntegrityBlockParsed(
      web_package::mojom::BundleIntegrityBlockPtr raw_integrity_block,
      web_package::mojom::BundleIntegrityBlockParseErrorPtr error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_EQ(state_, State::kInitializing);

    if (error) {
      FulfillWithError(UnusableSwbnFileError(std::move(error)));
      return;
    }

    ASSIGN_OR_RETURN(
        integrity_block_,
        web_package::SignedWebBundleIntegrityBlock::Create(
            std::move(raw_integrity_block))
            .transform_error([&](std::string error) {
              return UnusableSwbnFileError(
                  UnusableSwbnFileError::Error::
                      kIntegrityBlockParserFormatError,
                  "Error while parsing the Signed Web Bundle's integrity "
                  "block: " +
                      std::move(error));
            }),
        [&](auto error) { FulfillWithError(std::move(error)); });

    if (verify_signatures_) {
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(ReadLengthOfFile, file_->Duplicate()),
          base::BindOnce(&SignedWebBundleReaderImpl::OnFileLengthRead,
                         weak_ptr_factory_.GetWeakPtr()));
    } else {
      ReadMetadata();
    }
  }

  void OnFileLengthRead(
      base::expected<uint64_t, base::File::Error> file_length) {
    RETURN_IF_ERROR(file_length, [&](base::File::Error error) {
      FulfillWithError(UnusableSwbnFileError(
          UnusableSwbnFileError::Error::kIntegrityBlockParserInternalError,
          base::File::ErrorToString(error)));
    });

    CHECK(integrity_block_.has_value()) << "The integrity block must have been "
                                           "read before verifying signatures.";
    GetSignatureVerifier().VerifySignatures(
        file_->Duplicate(), *integrity_block_,
        base::BindOnce(&SignedWebBundleReaderImpl::OnSignaturesVerified,
                       weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                       *file_length));
  }

  void OnSignaturesVerified(
      const base::TimeTicks& verification_start_time,
      uint64_t file_length,
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
          FulfillWithError(UnusableSwbnFileError(error));
        });

    // Signatures are valid; continue with parsing of metadata.
    ReadMetadata();
  }

  void ReadMetadata() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_EQ(state_, State::kInitializing);

    CHECK(integrity_block_.has_value())
        << "The integrity block must have been read before reading metadata.";
    uint64_t metadata_offset = integrity_block_->size_in_bytes();

    parser_->ParseMetadata(
        metadata_offset,
        base::BindOnce(&SignedWebBundleReaderImpl::OnMetadataParsed,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnMetadataParsed(web_package::mojom::BundleMetadataPtr metadata,
                        web_package::mojom::BundleMetadataParseErrorPtr error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_EQ(state_, State::kInitializing);

    if (error) {
      FulfillWithError(UnusableSwbnFileError(error));
      return;
    }

    primary_url_ = metadata->primary_url;
    entries_ = std::move(metadata->requests);

    SetState(State::kInitialized);

    std::move(callback_).Run(base::ok());
  }

  void FulfillWithError(UnusableSwbnFileError error) {
    SetState(State::kError);
    Close(base::BindOnce(std::move(callback_),
                         base::unexpected(std::move(error))));
  }

  void ReadResponseInternal(
      web_package::mojom::BundleResponseLocationPtr location,
      ResponseCallback callback) {
    CHECK_EQ(state_, State::kInitialized);

    parser_->ParseResponse(
        location->offset, location->length,
        base::BindOnce(&SignedWebBundleReaderImpl::OnResponseParsed,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnResponseParsed(ResponseCallback callback,
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

  void StartReadingFromDataSource(
      mojo::DataPipeProducer* data_pipe_producer,
      ResponseBodyCallback callback,
      std::unique_ptr<mojo::DataPipeProducer::DataSource> data_source) {
    data_pipe_producer->Write(
        std::move(data_source),
        base::BindOnce(&SignedWebBundleReaderImpl::OnResponseBodyRead,
                       weak_ptr_factory_.GetWeakPtr(), data_pipe_producer,
                       std::move(callback)));
  }

  void OnResponseBodyRead(mojo::DataPipeProducer* producer,
                          ResponseBodyCallback callback,
                          MojoResult result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    active_response_body_producers_.erase(producer);
    net::Error net_result =
        result == MOJO_RESULT_OK ? net::Error::OK : net::Error::ERR_UNEXPECTED;
    std::move(callback).Run(net_result);
    ReplyClosedIfNecessary();
  }

  void ReplyClosedIfNecessary() {
    if (active_response_body_producers_.empty() && !close_callback_.is_null()) {
      std::move(close_callback_).Run();
    }
  }

  web_package::SignedWebBundleSignatureVerifier& GetSignatureVerifier() {
    if (g_signature_verifier_for_testing) {
      return *g_signature_verifier_for_testing;
    } else {
      return signature_verifier_;
    }
  }

  State state_ = State::kUninitialized;
  web_package::SignedWebBundleSignatureVerifier signature_verifier_;

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

  Callback callback_;
  base::OnceClosure close_callback_;

  bool verify_signatures_ = true;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SignedWebBundleReaderImpl> weak_ptr_factory_{this};
};

}  // namespace

void SignedWebBundleReader::Create(const base::FilePath& web_bundle_path,
                                   const std::optional<GURL>& base_url,
                                   bool verify_signatures,
                                   CreateCallback callback) {
  auto reader = std::make_unique<SignedWebBundleReaderImpl>(
      base::PassKey<SignedWebBundleReader>(), web_bundle_path, base_url,
      verify_signatures);
  auto* reader_ptr = reader.get();

  reader_ptr->Start(
      base::BindOnce(
          [](std::unique_ptr<SignedWebBundleReaderImpl> reader,
             base::expected<void, UnusableSwbnFileError> status) -> Result {
            RETURN_IF_ERROR(status);
            CHECK(reader->IsReady());
            return reader;
          },
          std::move(reader))
          .Then(std::move(callback)));
}

// static
base::AutoReset<web_package::SignedWebBundleSignatureVerifier*>
SignedWebBundleReader::SetSignatureVerifierForTesting(
    web_package::SignedWebBundleSignatureVerifier* verifier) {
  CHECK_IS_TEST();

  base::AutoReset<web_package::SignedWebBundleSignatureVerifier*> resetter(
      &g_signature_verifier_for_testing, verifier);
  return resetter;
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
