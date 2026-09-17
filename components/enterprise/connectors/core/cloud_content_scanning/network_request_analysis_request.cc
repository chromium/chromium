// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/network_request_analysis_request.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/barrier_callback.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/checked_math.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/connectors/core/features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"

namespace enterprise_connectors {

namespace {

// Returns the maximum body size that can be uploaded for cloud analysis. This
// mirrors the limit `FileAnalysisRequestBase` applies to file scans so that a
// given payload is treated consistently regardless of which request type
// carries it.
uint64_t GetMaxUploadSizeBytes() {
  if (base::FeatureList::IsEnabled(kEnableNewUploadSizeLimit)) {
    return 1024 * 1024 *
           static_cast<uint64_t>(kMaxContentAnalysisFileSizeMB.Get());
  }
  return BinaryUploadService::kMaxUploadSizeBytes;
}

// Returns the number of bytes a `network::DataElementFile` will actually
// contribute to the upload, or `std::nullopt` if that can't be determined.
//
// `DataElementFile::length()` can't be used directly because it's an upper
// bound on the range to upload rather than the real byte count:
//   - Blink appends whole-file elements (e.g. an unsliced `<input type=file>`)
//     with a length of `uint64_t::max()` as a "read to EOF" sentinel. Treating
//     that as a size would make every such upload look far larger than
//     `kMaxUploadSizeBytes`.
//   - Even a concrete length is only a requested range. The file may be
//     smaller, in which case fewer bytes are uploaded.
// The `min(file_size - offset, length)` below mirrors what the network stack
// computes at upload time in `UploadFileElementReader`, so the size scanned
// here matches the bytes that will actually be sent.
//
// This must run on a `MayBlock()` task runner since it stats the file.
std::optional<uint64_t> GetFileElementSizeBlocking(base::FilePath path,
                                                   uint64_t offset,
                                                   uint64_t length) {
  std::optional<int64_t> file_size = base::GetFileSize(path);
  if (file_size.has_value() && file_size.value() >= 0) {
    uint64_t actual_size = static_cast<uint64_t>(file_size.value());
    if (offset >= actual_size) {
      return 0;
    }
    return std::min(actual_size - offset, length);
  }
  // The file couldn't be stat'ed (e.g. it's missing or unreadable). Fall back
  // to the declared length when it's a real value, otherwise the size is
  // unknown.
  if (length != std::numeric_limits<uint64_t>::max()) {
    return length;
  }
  return std::nullopt;
}

}  // namespace

class NetworkRequestAnalysisRequest::DataPipeSizeGetter {
 public:
  DataPipeSizeGetter(
      mojo::PendingRemote<network::mojom::DataPipeGetter> pending_remote,
      base::OnceCallback<void(std::optional<uint64_t>)> callback)
      : remote_(std::move(pending_remote)), callback_(std::move(callback)) {}

  ~DataPipeSizeGetter() = default;

  void Start() {
    if (!remote_.is_bound() || !remote_.is_connected()) {
      Finish(std::nullopt);
      return;
    }
    remote_.set_disconnect_handler(base::BindOnce(
        &DataPipeSizeGetter::OnDisconnect, base::Unretained(this)));
    mojo::ScopedDataPipeProducerHandle producer;
    if (mojo::CreateDataPipe(nullptr, producer, consumer_) != MOJO_RESULT_OK) {
      Finish(std::nullopt);
      return;
    }
    remote_->Read(
        std::move(producer),
        base::BindOnce(&DataPipeSizeGetter::OnRead, base::Unretained(this)));
  }

 private:
  void OnRead(int32_t status, uint64_t size) {
 Finish(status == net::OK ? std::optional(size) : std::nullopt);
  }

  void OnDisconnect() { Finish(std::nullopt); }

  void Finish(std::optional<uint64_t> size) {
    remote_.reset();
    consumer_.reset();
    if (callback_) {
      std::move(callback_).Run(size);
    }
  }

  mojo::Remote<network::mojom::DataPipeGetter> remote_;
  mojo::ScopedDataPipeConsumerHandle consumer_;
  base::OnceCallback<void(std::optional<uint64_t>)> callback_;
};

NetworkRequestAnalysisRequest::NetworkRequestAnalysisRequest(
    CloudAnalysisSettings settings,
    scoped_refptr<network::ResourceRequestBody> request_body,
    BinaryUploadRequest::ContentAnalysisCallback callback,
    BrowserPolicyConnectorGetter policy_connector_getter)
    : BinaryUploadRequest(std::move(callback),
                          CloudOrLocalAnalysisSettings(std::move(settings)),
                          std::move(policy_connector_getter)),
      request_body_(std::move(request_body)) {
  CHECK(request_body_);

  IncrementCrashKey(ScanningCrashKey::PENDING_NETWORK_REQUESTS);
  IncrementCrashKey(ScanningCrashKey::TOTAL_NETWORK_REQUESTS);
}

NetworkRequestAnalysisRequest::~NetworkRequestAnalysisRequest() {
  DecrementCrashKey(ScanningCrashKey::PENDING_NETWORK_REQUESTS);
}

void NetworkRequestAnalysisRequest::GetRequestData(DataCallback callback) {
  if (result_.has_value()) {
    std::move(callback).Run(*result_, data_);
    return;
  }

  pending_callbacks_.push_back(std::move(callback));
  if (pending_callbacks_.size() > 1) {
    return;
  }

  if (request_body_->elements()->empty()) {
    CacheResultAndData(0);
    return;
  }

  // An overflow means the body is bigger than `uint64_t` can represent, which
  // is far past any upload limit. Saturate so it's reported as too large
  // rather than wrapping around to a small (or zero) size.
  base::CheckedNumeric<uint64_t> checked_bytes_sum = 0;
  size_t async_elements_count = 0;
  for (const auto& element : *request_body_->elements()) {
    switch (element.type()) {
      case network::DataElement::Tag::kChunkedDataPipe:
        // A chunked body is streamed with no length advertised up front, so
        // there's no way to bound it against the upload limit before sending.
        // Report it as too large rather than letting an arbitrarily large
        // body through the size gate unchecked.
        //
        // The size stays 0 because no byte count is available -- it is
        // deliberately *not* a claim that the body is empty. Consumers must
        // therefore key off `kFileTooLarge` rather than inferring anything
        // from `Data::size` being 0.
        CacheResultAndData(0, ScanRequestUploadResult::kFileTooLarge);
        return;
      case network::DataElement::Tag::kBytes:
        checked_bytes_sum +=
            element.As<network::DataElementBytes>().bytes().size();
        break;
      case network::DataElement::Tag::kFile:
      case network::DataElement::Tag::kDataPipe:
        ++async_elements_count;
        break;
    }
  }
  uint64_t sync_bytes_sum =
      checked_bytes_sum.ValueOrDefault(std::numeric_limits<uint64_t>::max());

  if (async_elements_count == 0) {
    CacheResultAndData(sync_bytes_sum);
    return;
  }

  auto barrier = base::BarrierCallback<std::optional<uint64_t>>(
      async_elements_count,
      base::BindOnce(&NetworkRequestAnalysisRequest::OnAllElementSizesComputed,
                     weak_factory_.GetWeakPtr(), sync_bytes_sum));

  std::vector<DataPipeSizeGetter*> getters_to_start;
  for (const auto& element : *request_body_->elements()) {
    switch (element.type()) {
      case network::DataElement::Tag::kFile: {
        const auto& file = element.As<network::DataElementFile>();
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
            base::BindOnce(&GetFileElementSizeBlocking, file.path(),
                           file.offset(), file.length()),
            base::OnceCallback<void(std::optional<uint64_t>)>(barrier));
        break;
      }
      case network::DataElement::Tag::kDataPipe: {
        auto getter = std::make_unique<DataPipeSizeGetter>(
            element.As<network::DataElementDataPipe>().CloneDataPipeGetter(),
            barrier);
        getters_to_start.push_back(getter.get());
        data_pipe_getters_.push_back(std::move(getter));
        break;
      }
      case network::DataElement::Tag::kBytes:
      case network::DataElement::Tag::kChunkedDataPipe:
        break;
    }
  }

  for (DataPipeSizeGetter* getter : getters_to_start) {
    getter->Start();
  }
}

void NetworkRequestAnalysisRequest::OnAllElementSizesComputed(
    uint64_t sync_bytes_sum,
    std::vector<std::optional<uint64_t>> async_sizes) {
  // The getters can't be destroyed synchronously here. This method is reached
  // through the barrier callback, which the final `DataPipeSizeGetter` runs
  // from inside its own `Finish()`, so destroying that getter now would free it
  // while it's still on the stack. Deferring also keeps the raw pointers held
  // by `GetRequestData`'s start loop valid, since a getter can complete the
  // barrier synchronously from `Start()`.
  //
  // The getters are bound into the task rather than passed to `DeleteSoon()`
  // so that they're freed even if the task never runs: `DeleteSoon()` binds a
  // raw pointer and intentionally leaks when its task is discarded, whereas
  // destroying this task destroys the bound getters with it.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(data_pipe_getters_)));
  data_pipe_getters_.clear();

  base::CheckedNumeric<uint64_t> total_size = sync_bytes_sum;
  for (const auto& size : async_sizes) {
    if (!size.has_value()) {
      // An element's size couldn't be determined (e.g. an unreadable file or a
      // data pipe that failed), so no total can be computed for the body and
      // it can't be checked against the upload limit. Report `kUnknown`, which
      // callers treat as "couldn't prepare this request" and which, per the
      // convention in `FileAnalysisRequestBase`, is paired with a zero size.
      CacheResultAndData(0, ScanRequestUploadResult::kUnknown);
      return;
    }
    total_size += size.value();
  }

  // As above, an overflow means the body is past any plausible limit, so
  // saturate instead of wrapping.
  CacheResultAndData(
      total_size.ValueOrDefault(std::numeric_limits<uint64_t>::max()));
}

void NetworkRequestAnalysisRequest::CacheResultAndData(uint64_t size) {
  CacheResultAndData(size, size > GetMaxUploadSizeBytes()
                               ? ScanRequestUploadResult::kFileTooLarge
                               : ScanRequestUploadResult::kSuccess);
}

void NetworkRequestAnalysisRequest::CacheResultAndData(
    uint64_t size,
    ScanRequestUploadResult result) {
  data_.size = size;
  data_.request_body = request_body_;
  result_ = result;

  // Snapshot the members before running any callback. A `DataCallback` is
  // allowed to destroy this request synchronously -- `OnGetRequestData` does
  // exactly that via `FinishAndCleanupRequest` whenever the result isn't
  // `kSuccess` -- so `result_` and `data_` must not be touched once the loop
  // has started. `callbacks` itself is a local and stays valid.
  const ScanRequestUploadResult cached_result = result;
  const Data cached_data = data_;

  auto callbacks = std::move(pending_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(cached_result, cached_data);
  }
}

}  // namespace enterprise_connectors
