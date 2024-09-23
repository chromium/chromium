// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/services/storage/service_worker/service_worker_resource_ops.h"

#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "base/pickle.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/public/cpp/big_io_buffer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/http/http_response_info.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/blob/blob_utils.h"

namespace storage {

namespace {

// Disk cache entry data indices.
//
// This enum pertains to data persisted on disk. Do not remove or reuse values.
enum {
  kResponseInfoIndex = 0,
  kResponseContentIndex = 1,
  kResponseMetadataIndex = 2,
};

// Convert an HttpResponseInfo retrieved from disk_cache to URLResponseHead.
network::mojom::URLResponseHeadPtr ConvertHttpResponseInfo(
    const net::HttpResponseInfo& http_info,
    int64_t response_data_size) {
  auto response_head = network::mojom::URLResponseHead::New();

  response_head->request_time = http_info.request_time;
  response_head->response_time = http_info.response_time;
  response_head->headers = http_info.headers;
  response_head->headers->GetMimeType(&response_head->mime_type);
  response_head->headers->GetCharset(&response_head->charset);
  response_head->content_length = response_data_size;
  response_head->was_fetched_via_spdy = http_info.was_fetched_via_spdy;
  response_head->was_alpn_negotiated = http_info.was_alpn_negotiated;
  response_head->connection_info = http_info.connection_info;
  response_head->alpn_negotiated_protocol = http_info.alpn_negotiated_protocol;
  response_head->remote_endpoint = http_info.remote_endpoint;
  response_head->cert_status = http_info.ssl_info.cert_status;
  // See ConvertToPickle(), where an invalid ssl_info is put into storage in
  // case of |response_head->ssl_info| being nullptr. Here we restore
  // |response_head->ssl_info| to nullptr if we got the invalid ssl_info from
  // storage.
  if (http_info.ssl_info.is_valid())
    response_head->ssl_info = http_info.ssl_info;

  return response_head;
}

// Convert a URLResponseHead to base::Pickle. Used to persist the response to
// disk.
std::unique_ptr<base::Pickle> ConvertToPickle(
    network::mojom::URLResponseHeadPtr response_head) {
  net::HttpResponseInfo response_info;
  response_info.headers = response_head->headers;
  if (response_head->ssl_info.has_value()) {
    response_info.ssl_info = *response_head->ssl_info;
    DCHECK(response_info.ssl_info.is_valid());
  }
  response_info.was_fetched_via_spdy = response_head->was_fetched_via_spdy;
  response_info.was_alpn_negotiated = response_head->was_alpn_negotiated;
  response_info.alpn_negotiated_protocol =
      response_head->alpn_negotiated_protocol;
  response_info.connection_info = response_head->connection_info;
  response_info.remote_endpoint = response_head->remote_endpoint;
  response_info.response_time = response_head->response_time;

  const bool kSkipTransientHeaders = true;
  const bool kTruncated = false;
  auto pickle = std::make_unique<base::Pickle>();
  response_info.Persist(pickle.get(), kSkipTransientHeaders, kTruncated);
  return pickle;
}

// An IOBuffer that wraps a pickle's data. Used to write URLResponseHead.
class WrappedPickleIOBuffer : public net::WrappedIOBuffer {
 public:
  explicit WrappedPickleIOBuffer(std::unique_ptr<const base::Pickle> pickle)
      : net::WrappedIOBuffer(*pickle), pickle_(std::move(pickle)) {
    DCHECK(pickle_->data());
  }

  size_t size() const { return pickle_->size(); }

 private:
  ~WrappedPickleIOBuffer() override {
    // `data_` is a pointer on `pickle_` and should be nullified before that to
    // prevent it from being dangling.
    data_ = nullptr;
  }

  const std::unique_ptr<const base::Pickle> pickle_;
};

}  // namespace

DiskEntryCreator::DiskEntryCreator(
    int64_t resource_id,
    base::WeakPtr<ServiceWorkerDiskCache> disk_cache)
    : resource_id_(resource_id), disk_cache_(std::move(disk_cache)) {
  DCHECK_NE(resource_id_, blink::mojom::kInvalidServiceWorkerResourceId);
  DCHECK(disk_cache_);
}

DiskEntryCreator::~DiskEntryCreator() = default;

void DiskEntryCreator::EnsureEntryIsCreated(base::OnceClosure callback) {
  DCHECK(creation_phase_ == CreationPhase::kNoAttempt ||
         creation_phase_ == CreationPhase::kDone);
  DCHECK(!ensure_entry_is_created_callback_);
  ensure_entry_is_created_callback_ = std::move(callback);

  if (entry_) {
    RunEnsureEntryIsCreatedCallback();
    return;
  }

  if (!disk_cache_) {
    entry_.reset();
    RunEnsureEntryIsCreatedCallback();
    return;
  }

  creation_phase_ = CreationPhase::kInitialAttempt;
  disk_cache_->CreateEntry(
      resource_id_,
      base::BindOnce(&DiskEntryCreator::DidCreateEntryForFirstAttempt,
                     weak_factory_.GetWeakPtr()));
}

void DiskEntryCreator::DidCreateEntryForFirstAttempt(
    int rv,
    std::unique_ptr<ServiceWorkerDiskCacheEntry> entry) {
  DCHECK_EQ(creation_phase_, CreationPhase::kInitialAttempt);
  DCHECK(!entry_);

  if (!disk_cache_) {
    entry_.reset();
    RunEnsureEntryIsCreatedCallback();
    return;
  }

  if (rv != net::OK) {
    // The first attempt to create an entry is failed. Try to overwrite the
    // existing entry.
    creation_phase_ = CreationPhase::kDoomExisting;
    disk_cache_->DoomEntry(
        resource_id_, base::BindOnce(&DiskEntryCreator::DidDoomExistingEntry,
                                     weak_factory_.GetWeakPtr()));
    return;
  }

  DCHECK(entry);
  entry_ = std::move(entry);
  RunEnsureEntryIsCreatedCallback();
}

void DiskEntryCreator::DidDoomExistingEntry(int rv) {
  DCHECK_EQ(creation_phase_, CreationPhase::kDoomExisting);
  DCHECK(!entry_);

  if (!disk_cache_) {
    entry_.reset();
    RunEnsureEntryIsCreatedCallback();
    return;
  }

  creation_phase_ = CreationPhase::kSecondAttempt;
  disk_cache_->CreateEntry(
      resource_id_,
      base::BindOnce(&DiskEntryCreator::DidCreateEntryForSecondAttempt,
                     weak_factory_.GetWeakPtr()));
}

void DiskEntryCreator::DidCreateEntryForSecondAttempt(
    int rv,
    std::unique_ptr<ServiceWorkerDiskCacheEntry> entry) {
  DCHECK_EQ(creation_phase_, CreationPhase::kSecondAttempt);

  if (!disk_cache_) {
    entry_.reset();
    RunEnsureEntryIsCreatedCallback();
    return;
  }

  if (rv != net::OK) {
    // The second attempt is also failed. Give up creating an entry.
    entry_.reset();
    RunEnsureEntryIsCreatedCallback();
    return;
  }

  DCHECK(!entry_);
  DCHECK(entry);
  entry_ = std::move(entry);
  RunEnsureEntryIsCreatedCallback();
}

void DiskEntryCreator::RunEnsureEntryIsCreatedCallback() {
  creation_phase_ = CreationPhase::kDone;
  std::move(ensure_entry_is_created_callback_).Run();
}

DiskEntryOpener::DiskEntryOpener(
    int64_t resource_id,
    base::WeakPtr<ServiceWorkerDiskCache> disk_cache)
    : resource_id_(resource_id), disk_cache_(std::move(disk_cache)) {
  DCHECK_NE(resource_id_, blink::mojom::kInvalidServiceWorkerResourceId);
  DCHECK(disk_cache_);
}

DiskEntryOpener::~DiskEntryOpener() = default;

void DiskEntryOpener::EnsureEntryIsOpen(base::OnceClosure callback) {
  if (entry_) {
    std::move(callback).Run();
    return;
  }

  if (!disk_cache_) {
    std::move(callback).Run();
    return;
  }

  disk_cache_->OpenEntry(
      resource_id_,
      base::BindOnce(&DiskEntryOpener::DidOpenEntry, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void DiskEntryOpener::DidOpenEntry(
    base::OnceClosure callback,
    int rv,
    std::unique_ptr<ServiceWorkerDiskCacheEntry> entry) {
  if (!entry_ && rv == net::OK) {
    DCHECK(entry);
    entry_ = std::move(entry);
  }

  std::move(callback).Run();
}

class ServiceWorkerResourceReaderImpl::DataReader {
 public:
  DataReader(base::WeakPtr<ServiceWorkerResourceReaderImpl> owner,
             size_t total_bytes_to_read,
             mojo::ScopedDataPipeProducerHandle producer_handle)
      : owner_(std::move(owner)),
        total_bytes_to_read_(total_bytes_to_read),
        producer_handle_(std::move(producer_handle)),
        watcher_(FROM_HERE,
                 mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                 base::SequencedTaskRunner::GetCurrentDefault()) {
    DCHECK(owner_);
  }
  ~DataReader() = default;

  DataReader(const DataReader&) = delete;
  DataReader operator=(const DataReader&) = delete;

  void Start(ReadDataCallback callback) {
#if DCHECK_IS_ON()
    DCHECK_EQ(state_, State::kInitialized);
    state_ = State::kStarted;
#endif
    DCHECK(!callback_);
    callback_ = std::move(callback);

    owner_->entry_opener_.EnsureEntryIsOpen(base::BindOnce(
        &DataReader::ContinueReadData, weak_factory_.GetWeakPtr()));
  }

 private:
  void ContinueReadData() {
#if DCHECK_IS_ON()
    DCHECK_EQ(state_, State::kStarted);
    state_ = State::kCacheEntryOpened;
#endif

    if (!owner_) {
      Complete(net::ERR_ABORTED);
      return;
    }

    if (!owner_->entry_opener_.entry()) {
      Complete(net::ERR_CACHE_MISS);
      return;
    }

    watcher_.Watch(producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                   base::BindRepeating(&DataReader::OnWritable,
                                       weak_factory_.GetWeakPtr()));
    watcher_.ArmOrNotify();
  }

  void OnWritable(MojoResult) {
#if DCHECK_IS_ON()
    DCHECK(state_ == State::kCacheEntryOpened || state_ == State::kDataRead);
    state_ = State::kProducerWritable;
#endif

    DCHECK(producer_handle_.is_valid());
    DCHECK(!pending_buffer_);

    if (!owner_ || !owner_->entry_opener_.entry()) {
      Complete(net::ERR_ABORTED);
      return;
    }

    MojoResult rv = network::NetToMojoPendingBuffer::BeginWrite(
        &producer_handle_, &pending_buffer_);
    switch (rv) {
      case MOJO_RESULT_INVALID_ARGUMENT:
      case MOJO_RESULT_BUSY:
        NOTREACHED_IN_MIGRATION();
        return;
      case MOJO_RESULT_FAILED_PRECONDITION:
        Complete(net::ERR_ABORTED);
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        watcher_.ArmOrNotify();
        return;
      case MOJO_RESULT_OK:
        // |producer__handle_| must have been taken by |pending_buffer_|.
        DCHECK(pending_buffer_);
        DCHECK(!producer_handle_.is_valid());
        break;
    }

    uint32_t num_bytes = pending_buffer_->size();
    num_bytes = std::min(num_bytes, blink::BlobUtils::GetDataPipeChunkSize());
    auto buffer =
        base::MakeRefCounted<network::NetToMojoIOBuffer>(pending_buffer_);

    net::IOBuffer* raw_buffer = buffer.get();
    int read_bytes = owner_->entry_opener_.entry()->Read(
        kResponseContentIndex, current_bytes_read_, raw_buffer, num_bytes,
        base::BindOnce(&DataReader::DidReadData, weak_factory_.GetWeakPtr(),
                       buffer));
    if (read_bytes != net::ERR_IO_PENDING) {
      DidReadData(std::move(buffer), read_bytes);
    }
  }

  void DidReadData(scoped_refptr<network::NetToMojoIOBuffer> buffer,
                   int read_bytes) {
#if DCHECK_IS_ON()
    DCHECK_EQ(state_, State::kProducerWritable);
    state_ = State::kDataRead;
#endif

    if (read_bytes < 0) {
      Complete(read_bytes);
      return;
    }

    producer_handle_ = pending_buffer_->Complete(read_bytes);
    DCHECK(producer_handle_.is_valid());
    pending_buffer_.reset();
    current_bytes_read_ += read_bytes;

    if (read_bytes == 0 || current_bytes_read_ == total_bytes_to_read_) {
      // All data has been read.
      Complete(current_bytes_read_);
      return;
    }
    watcher_.ArmOrNotify();
  }

  void Complete(int status) {
#if DCHECK_IS_ON()
    DCHECK_NE(state_, State::kInitialized);
    DCHECK_NE(state_, State::kComplete);
    state_ = State::kComplete;
#endif

    watcher_.Cancel();
    producer_handle_.reset();

    if (callback_) {
      std::move(callback_).Run(status);
    }

    if (owner_) {
      owner_->DidReadDataComplete();
    }
  }

  base::WeakPtr<ServiceWorkerResourceReaderImpl> owner_;
  const size_t total_bytes_to_read_;
  size_t current_bytes_read_ = 0;
  ReadDataCallback callback_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  mojo::SimpleWatcher watcher_;
  scoped_refptr<network::NetToMojoPendingBuffer> pending_buffer_;

#if DCHECK_IS_ON()
  enum class State {
    kInitialized,
    kStarted,
    kCacheEntryOpened,
    kProducerWritable,
    kDataRead,
    kComplete,
  };
  State state_ = State::kInitialized;
#endif  // DCHECK_IS_ON()

  base::WeakPtrFactory<DataReader> weak_factory_{this};
};

ServiceWorkerResourceReaderImpl::ServiceWorkerResourceReaderImpl(
    int64_t resource_id,
    base::WeakPtr<ServiceWorkerDiskCache> disk_cache,
    mojo::PendingReceiver<mojom::ServiceWorkerResourceReader> receiver,
    base::OnceClosure disconnect_handler)
    : entry_opener_(resource_id, std::move(disk_cache)),
      receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(std::move(disconnect_handler));
}

ServiceWorkerResourceReaderImpl::~ServiceWorkerResourceReaderImpl() = default;

void ServiceWorkerResourceReaderImpl::ReadResponseHead(
    ReadResponseHeadCallback callback) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kReadResponseHeadStarted;
#endif
  DCHECK(!read_response_head_callback_) << __func__ << " already called";
  DCHECK(!response_head_) << " another ReadResponseHead() in progress";
  DCHECK(!metadata_buffer_);
  DCHECK(!data_reader_);

  read_response_head_callback_ = std::move(callback);
  entry_opener_.EnsureEntryIsOpen(
      base::BindOnce(&ServiceWorkerResourceReaderImpl::ContinueReadResponseHead,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerResourceReaderImpl::PrepareReadData(
    int64_t size,
    PrepareReadDataCallback callback) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kReadDataPrepared;
#endif
  DCHECK(!read_response_head_callback_) << "ReadResponseHead() being operating";
  DCHECK(!response_head_);
  DCHECK(!metadata_buffer_);
  DCHECK(!data_reader_);

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = blink::BlobUtils::GetDataPipeCapacity(size);

  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  MojoResult rv =
      mojo::CreateDataPipe(&options, producer_handle, consumer_handle);
  if (rv != MOJO_RESULT_OK) {
    std::move(callback).Run(mojo::ScopedDataPipeConsumerHandle());
    return;
  }

  data_reader_ = std::make_unique<DataReader>(weak_factory_.GetWeakPtr(), size,
                                              std::move(producer_handle));
  std::move(callback).Run(std::move(consumer_handle));
}

void ServiceWorkerResourceReaderImpl::ReadData(ReadDataCallback callback) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kReadDataPrepared);
  state_ = State::kReadDataStarted;
#endif
  data_reader_->Start(std::move(callback));
}

void ServiceWorkerResourceReaderImpl::ContinueReadResponseHead() {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kReadResponseHeadStarted);
  state_ = State::kCacheEntryOpened;
#endif
  DCHECK(read_response_head_callback_);

  if (!entry_opener_.entry()) {
    FailReadResponseHead(net::ERR_CACHE_MISS);
    return;
  }

  int64_t size = entry_opener_.entry()->GetSize(kResponseInfoIndex);
  if (size <= 0) {
    FailReadResponseHead(net::ERR_CACHE_MISS);
    return;
  }

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(
      base::checked_cast<size_t>(size));
  int rv = entry_opener_.entry()->Read(
      kResponseInfoIndex, /*offset=*/0, buffer.get(), size,
      base::BindOnce(&ServiceWorkerResourceReaderImpl::DidReadHttpResponseInfo,
                     weak_factory_.GetWeakPtr(), buffer));
  if (rv != net::ERR_IO_PENDING) {
    DidReadHttpResponseInfo(std::move(buffer), rv);
  }
}

void ServiceWorkerResourceReaderImpl::DidReadHttpResponseInfo(
    scoped_refptr<net::IOBuffer> buffer,
    int status) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kCacheEntryOpened);
  state_ = State::kResponseInfoRead;
#endif
  DCHECK(read_response_head_callback_);
  DCHECK(entry_opener_.entry());

  if (status < 0) {
    FailReadResponseHead(status);
    return;
  }

  // Deserialize the http info structure, ensuring we got headers.
  base::Pickle pickle = base::Pickle::WithUnownedBuffer(base::as_bytes(
      base::span(buffer->data(), base::checked_cast<size_t>(status))));
  auto http_info = std::make_unique<net::HttpResponseInfo>();
  bool response_truncated = false;
  if (!http_info->InitFromPickle(pickle, &response_truncated) ||
      !http_info->headers.get()) {
    FailReadResponseHead(net::ERR_FAILED);
    return;
  }
  DCHECK(!response_truncated);

  int64_t response_data_size =
      entry_opener_.entry()->GetSize(kResponseContentIndex);

  response_head_ = ConvertHttpResponseInfo(*http_info, response_data_size);

  int64_t metadata_size =
      entry_opener_.entry()->GetSize(kResponseMetadataIndex);
  DCHECK_GE(metadata_size, 0);
  if (metadata_size <= 0) {
    CompleteReadResponseHead(status);
    return;
  }

  // Read metadata.
  metadata_buffer_ = base::MakeRefCounted<BigIOBuffer>(
      mojo_base::BigBuffer(base::checked_cast<size_t>(metadata_size)));
  int rv = entry_opener_.entry()->Read(
      kResponseMetadataIndex, /*offset=*/0, metadata_buffer_.get(),
      metadata_size,
      base::BindOnce(&ServiceWorkerResourceReaderImpl::DidReadMetadata,
                     weak_factory_.GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING) {
    DidReadMetadata(rv);
  }
}

void ServiceWorkerResourceReaderImpl::DidReadMetadata(int status) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kResponseInfoRead);
  state_ = State::kMetadataRead;
#endif
  DCHECK(read_response_head_callback_);
  DCHECK(metadata_buffer_);

  if (status < 0) {
    FailReadResponseHead(status);
    return;
  }

  CompleteReadResponseHead(status);
}

void ServiceWorkerResourceReaderImpl::FailReadResponseHead(int status) {
  DCHECK_NE(net::OK, status);
  response_head_ = nullptr;
  metadata_buffer_ = nullptr;
  CompleteReadResponseHead(status);
}

void ServiceWorkerResourceReaderImpl::CompleteReadResponseHead(int status) {
#if DCHECK_IS_ON()
  DCHECK_NE(state_, State::kIdle);
  state_ = State::kIdle;
#endif
  DCHECK(read_response_head_callback_);

  std::optional<mojo_base::BigBuffer> metadata =
      metadata_buffer_
          ? std::optional<mojo_base::BigBuffer>(metadata_buffer_->TakeBuffer())
          : std::nullopt;

  metadata_buffer_ = nullptr;

  std::move(read_response_head_callback_)
      .Run(status, std::move(response_head_), std::move(metadata));
}

void ServiceWorkerResourceReaderImpl::DidReadDataComplete() {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kReadDataStarted);
  state_ = State::kIdle;
#endif
  DCHECK(data_reader_);
  data_reader_.reset();
}

ServiceWorkerResourceWriterImpl::ServiceWorkerResourceWriterImpl(
    int64_t resource_id,
    base::WeakPtr<ServiceWorkerDiskCache> disk_cache,
    mojo::PendingReceiver<mojom::ServiceWorkerResourceWriter> receiver,
    base::OnceClosure disconnect_handler)
    : entry_creator_(resource_id, std::move(disk_cache)),
      receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(std::move(disconnect_handler));
}

ServiceWorkerResourceWriterImpl::~ServiceWorkerResourceWriterImpl() = default;

void ServiceWorkerResourceWriterImpl::WriteResponseHead(
    network::mojom::URLResponseHeadPtr response_head,
    WriteResponseHeadCallback callback) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kWriteResponseHeadStarted;
#endif
  entry_creator_.EnsureEntryIsCreated(
      base::BindOnce(&ServiceWorkerResourceWriterImpl::WriteResponseHeadToEntry,
                     weak_factory_.GetWeakPtr(), std::move(response_head),
                     std::move(callback)));
}

void ServiceWorkerResourceWriterImpl::WriteData(mojo_base::BigBuffer data,
                                                WriteDataCallback callback) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kWriteDataStarted;
#endif
  entry_creator_.EnsureEntryIsCreated(base::BindOnce(
      &ServiceWorkerResourceWriterImpl::WriteDataToEntry,
      weak_factory_.GetWeakPtr(), std::move(data), std::move(callback)));
}

void ServiceWorkerResourceWriterImpl::WriteResponseHeadToEntry(
    network::mojom::URLResponseHeadPtr response_head,
    WriteResponseHeadCallback callback) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kWriteResponseHeadStarted);
  state_ = State::kWriteResponseHeadHasEntry;
#endif
  if (!entry_creator_.entry()) {
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }

  DCHECK(!write_callback_);
  write_callback_ = std::move(callback);

  std::unique_ptr<const base::Pickle> pickle =
      ConvertToPickle(std::move(response_head));
  auto buffer = base::MakeRefCounted<WrappedPickleIOBuffer>(std::move(pickle));

  size_t write_amount = buffer->size();
  int rv = entry_creator_.entry()->Write(
      kResponseInfoIndex, /*offset=*/0, buffer.get(), write_amount,
      base::BindOnce(&ServiceWorkerResourceWriterImpl::DidWriteResponseHead,
                     weak_factory_.GetWeakPtr(), buffer, write_amount));
  if (rv != net::ERR_IO_PENDING) {
    DidWriteResponseHead(std::move(buffer), write_amount, rv);
  }
}

void ServiceWorkerResourceWriterImpl::DidWriteResponseHead(
    scoped_refptr<net::IOBuffer> buffer,
    size_t write_amount,
    int rv) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kWriteResponseHeadHasEntry);
  state_ = State::kIdle;
#endif
  DCHECK(write_callback_);
  DCHECK(rv < 0 || base::checked_cast<size_t>(rv) == write_amount);
  std::move(write_callback_).Run(rv);
}

void ServiceWorkerResourceWriterImpl::WriteDataToEntry(
    mojo_base::BigBuffer data,
    WriteDataCallback callback) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kWriteDataStarted);
  state_ = State::kWriteDataHasEntry;
#endif
  if (!entry_creator_.entry()) {
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }

  DCHECK(!write_callback_);
  write_callback_ = std::move(callback);

  size_t write_amount = data.size();
  auto buffer = base::MakeRefCounted<BigIOBuffer>(std::move(data));
  int rv = entry_creator_.entry()->Write(
      kResponseContentIndex, write_position_, buffer.get(), write_amount,
      base::BindOnce(&ServiceWorkerResourceWriterImpl::DidWriteData,
                     weak_factory_.GetWeakPtr(), buffer, write_amount));
  if (rv != net::ERR_IO_PENDING) {
    DidWriteData(std::move(buffer), write_amount, rv);
  }
}

void ServiceWorkerResourceWriterImpl::DidWriteData(
    scoped_refptr<net::IOBuffer> buffer,
    size_t write_amount,
    int rv) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kWriteDataHasEntry);
  state_ = State::kIdle;
#endif
  DCHECK(write_callback_);
  if (rv >= 0) {
    DCHECK(base::checked_cast<size_t>(rv) == write_amount);
    write_position_ += write_amount;
  }
  std::move(write_callback_).Run(rv);
}

ServiceWorkerResourceMetadataWriterImpl::
    ServiceWorkerResourceMetadataWriterImpl(
        int64_t resource_id,
        base::WeakPtr<ServiceWorkerDiskCache> disk_cache,
        mojo::PendingReceiver<mojom::ServiceWorkerResourceMetadataWriter>
            receiver,
        base::OnceClosure disconnect_handler)
    : entry_opener_(resource_id, std::move(disk_cache)),
      receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(std::move(disconnect_handler));
}

ServiceWorkerResourceMetadataWriterImpl::
    ~ServiceWorkerResourceMetadataWriterImpl() = default;

void ServiceWorkerResourceMetadataWriterImpl::WriteMetadata(
    mojo_base::BigBuffer data,
    WriteMetadataCallback callback) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kWriteMetadataStarted;
#endif
  entry_opener_.EnsureEntryIsOpen(base::BindOnce(
      &ServiceWorkerResourceMetadataWriterImpl::ContinueWriteMetadata,
      weak_factory_.GetWeakPtr(), std::move(data), std::move(callback)));
}

void ServiceWorkerResourceMetadataWriterImpl::ContinueWriteMetadata(
    mojo_base::BigBuffer data,
    WriteMetadataCallback callback) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kWriteMetadataStarted);
  state_ = State::kWriteMetadataHasEntry;
#endif
  if (!entry_opener_.entry()) {
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }

  DCHECK(!write_metadata_callback_);
  write_metadata_callback_ = std::move(callback);
  size_t write_amount = data.size();
  auto buffer = base::MakeRefCounted<BigIOBuffer>(std::move(data));
  int rv = entry_opener_.entry()->Write(
      kResponseMetadataIndex, /*offset=*/0, buffer.get(), write_amount,
      base::BindOnce(&ServiceWorkerResourceMetadataWriterImpl::DidWriteMetadata,
                     weak_factory_.GetWeakPtr(), buffer, write_amount));
  if (rv != net::ERR_IO_PENDING) {
    DidWriteMetadata(std::move(buffer), write_amount, rv);
  }
}

void ServiceWorkerResourceMetadataWriterImpl::DidWriteMetadata(
    scoped_refptr<net::IOBuffer> buffer,
    size_t write_amount,
    int rv) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kWriteMetadataHasEntry);
  state_ = State::kIdle;
#endif
  DCHECK(rv < 0 || base::checked_cast<size_t>(rv) == write_amount);
  DCHECK(write_metadata_callback_);
  std::move(write_metadata_callback_).Run(rv);
}

}  // namespace storage
