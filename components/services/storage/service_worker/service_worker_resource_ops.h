// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_OPS_H_
#define COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_OPS_H_

#include "base/memory/weak_ptr.h"
#include "components/services/storage/public/cpp/big_io_buffer.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "components/services/storage/service_worker/service_worker_disk_cache.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace storage {

// Creates and owns a service worker disk cacke entry.
class DiskEntryCreator {
 public:
  DiskEntryCreator(int64_t resource_id,
                   base::WeakPtr<ServiceWorkerDiskCache> disk_cache);
  ~DiskEntryCreator();

  DiskEntryCreator(const DiskEntryCreator&) = delete;
  DiskEntryCreator& operator=(const DiskEntryCreator&) = delete;

  // Can be nullptr when a disk cache error occurs.
  ServiceWorkerDiskCacheEntry* entry() {
    DCHECK_EQ(creation_phase_, CreationPhase::kDone);
    return entry_.get();
  }

  // Calls the callback when entry() is created and can be used.
  //
  // Overlapping calls are not allowed. Specifically, once the method is called,
  // it must not be called again until it calls the callback.
  //
  // If necessary, kicks off the creation of a disk cache entry for the
  // `resource_id` passed to the constructor. After the callback is called,
  // `entry()` can be safely called to obtain the created entry.
  //
  // Has a retry mechanism. If the first attempt fails, dooms the existing
  // entry, then tries to create an entry again.
  void EnsureEntryIsCreated(base::OnceClosure callback);

 private:
  // State of creating a disk_cache entry.
  enum class CreationPhase {
    kNoAttempt,
    kInitialAttempt,
    kDoomExisting,
    kSecondAttempt,
    kDone,
  };

  // Callbacks of EnsureEntryIsCreated().
  void DidCreateEntryForFirstAttempt(
      int rv,
      std::unique_ptr<ServiceWorkerDiskCacheEntry> entry);
  void DidDoomExistingEntry(int rv);
  void DidCreateEntryForSecondAttempt(
      int rv,
      std::unique_ptr<ServiceWorkerDiskCacheEntry> entry);

  void RunEnsureEntryIsCreatedCallback();

  const int64_t resource_id_;
  base::WeakPtr<ServiceWorkerDiskCache> disk_cache_;
  std::unique_ptr<ServiceWorkerDiskCacheEntry> entry_;

  CreationPhase creation_phase_ = CreationPhase::kNoAttempt;

  // Stored as a data member to handle //net-style maybe-async methods.
  base::OnceClosure ensure_entry_is_created_callback_;

  base::WeakPtrFactory<DiskEntryCreator> weak_factory_{this};
};

// Opens and owns a service worker disk cache entry.
class DiskEntryOpener {
 public:
  DiskEntryOpener(int64_t resource_id,
                  base::WeakPtr<ServiceWorkerDiskCache> disk_cache);
  ~DiskEntryOpener();

  DiskEntryOpener(const DiskEntryOpener&) = delete;
  DiskEntryOpener& operator=(const DiskEntryOpener&) = delete;

  // Can be nullptr when a disk cache error occurs.
  ServiceWorkerDiskCacheEntry* entry() { return entry_.get(); }

  // Calls the callback when entry() is opened and can be used.
  //
  // If necessary, opens a disk cache entry for the `resource_id` passed to the
  // constructor. After the callback is called, `entry()` can be safely called
  // to obtain the created entry.
  void EnsureEntryIsOpen(base::OnceClosure callback);

 private:
  void DidOpenEntry(base::OnceClosure callback,
                    int rv,
                    std::unique_ptr<ServiceWorkerDiskCacheEntry> entry);

  const int64_t resource_id_;
  base::WeakPtr<ServiceWorkerDiskCache> disk_cache_;
  std::unique_ptr<ServiceWorkerDiskCacheEntry> entry_;

  base::WeakPtrFactory<DiskEntryOpener> weak_factory_{this};
};

// The implementation of mojom::ServiceWorkerResourceReader.
class ServiceWorkerResourceReaderImpl
    : public mojom::ServiceWorkerResourceReader {
 public:
  ServiceWorkerResourceReaderImpl(
      int64_t resource_id,
      base::WeakPtr<ServiceWorkerDiskCache> disk_cache,
      mojo::PendingReceiver<mojom::ServiceWorkerResourceReader> receiver,
      base::OnceClosure disconnect_handler);

  ServiceWorkerResourceReaderImpl(const ServiceWorkerResourceReaderImpl&) =
      delete;
  ServiceWorkerResourceReaderImpl& operator=(
      const ServiceWorkerResourceReaderImpl&) = delete;

  ~ServiceWorkerResourceReaderImpl() override;

  // mojom::ServiceWorkerResourceReader implementations:
  void ReadResponseHead(ReadResponseHeadCallback callback) override;
  void PrepareReadData(int64_t size, PrepareReadDataCallback callback) override;
  void ReadData(ReadDataCallback callback) override;

 private:
  class DataReader;

  // Called while executing ReadResponseHead() in the order they are declared.
  void ContinueReadResponseHead();
  void DidReadHttpResponseInfo(scoped_refptr<net::IOBuffer> buffer, int status);
  void DidReadMetadata(int status);
  // Complete the operation started by ReadResponseHead().
  void FailReadResponseHead(int status);
  void CompleteReadResponseHead(int status);

  // Completes ReadData(). Called when `data_reader_` finished reading response
  // data.
  void DidReadDataComplete();

  DiskEntryOpener entry_opener_;

  // Used to read metadata from disk cache.
  scoped_refptr<BigIOBuffer> metadata_buffer_;
  // Holds the return value of ReadResponseHead(). Stored as a member field
  // to handle //net-style maybe-async methods.
  network::mojom::URLResponseHeadPtr response_head_;
  // Holds the callback of ReadResponseHead(). Stored as a member field to
  // handle //net-style maybe-async methods.
  ReadResponseHeadCallback read_response_head_callback_;

  // Helper for ReadData().
  std::unique_ptr<DataReader> data_reader_;

#if DCHECK_IS_ON()
  enum class State {
    kIdle,
    kReadResponseHeadStarted,
    kReadDataPrepared,
    kReadDataStarted,
    kCacheEntryOpened,
    kResponseInfoRead,
    kMetadataRead,
  };
  State state_ = State::kIdle;
#endif  // DCHECK_IS_ON()

  mojo::Receiver<mojom::ServiceWorkerResourceReader> receiver_;

  base::WeakPtrFactory<ServiceWorkerResourceReaderImpl> weak_factory_{this};
};

// The implementation of mojom::ServiceWorkerResourceWriter.
class ServiceWorkerResourceWriterImpl
    : public mojom::ServiceWorkerResourceWriter {
 public:
  ServiceWorkerResourceWriterImpl(
      int64_t resource_id,
      base::WeakPtr<ServiceWorkerDiskCache> disk_cache,
      mojo::PendingReceiver<mojom::ServiceWorkerResourceWriter> receiver,
      base::OnceClosure disconnect_handler);

  ServiceWorkerResourceWriterImpl(const ServiceWorkerResourceWriterImpl&) =
      delete;
  ServiceWorkerResourceWriterImpl& operator=(
      const ServiceWorkerResourceWriterImpl&) = delete;

  ~ServiceWorkerResourceWriterImpl() override;

  // mojom::ServiceWorkerResourceWriter implementations:
  void WriteResponseHead(network::mojom::URLResponseHeadPtr response_head,
                         WriteResponseHeadCallback callback) override;
  void WriteData(mojo_base::BigBuffer data,
                 WriteDataCallback callback) override;

 private:
  // Called while executing WriteResponseHead().
  void WriteResponseHeadToEntry(
      network::mojom::URLResponseHeadPtr response_head,
      WriteResponseHeadCallback callback);
  void DidWriteResponseHead(scoped_refptr<net::IOBuffer> buffer,
                            size_t write_amount,
                            int rv);

  // Called while executing WriteData().
  void WriteDataToEntry(mojo_base::BigBuffer data, WriteDataCallback callback);
  void DidWriteData(scoped_refptr<net::IOBuffer> buffer,
                    size_t write_amount,
                    int rv);

  DiskEntryCreator entry_creator_;

  // Points the current write position of WriteData().
  size_t write_position_ = 0;

  // Holds the callback of WriteResponseHead() or WriteData(). Stored as a data
  // member to handle //net-style maybe-async methods.
  net::CompletionOnceCallback write_callback_;

#if DCHECK_IS_ON()
  enum class State {
    kIdle,
    kWriteResponseHeadStarted,
    kWriteResponseHeadHasEntry,
    kWriteDataStarted,
    kWriteDataHasEntry,
  };
  State state_ = State::kIdle;
#endif  // DCHECK_IS_ON()

  mojo::Receiver<mojom::ServiceWorkerResourceWriter> receiver_;

  base::WeakPtrFactory<ServiceWorkerResourceWriterImpl> weak_factory_{this};
};

// The implementation of mojom::ServiceWorkerResourceMetadataWriter.
class ServiceWorkerResourceMetadataWriterImpl
    : public mojom::ServiceWorkerResourceMetadataWriter {
 public:
  ServiceWorkerResourceMetadataWriterImpl(
      int64_t resource_id,
      base::WeakPtr<ServiceWorkerDiskCache> disk_cache,
      mojo::PendingReceiver<mojom::ServiceWorkerResourceMetadataWriter>
          receiver,
      base::OnceClosure disconnect_handler);

  ServiceWorkerResourceMetadataWriterImpl(
      const ServiceWorkerResourceMetadataWriterImpl&) = delete;
  ServiceWorkerResourceMetadataWriterImpl& operator=(
      const ServiceWorkerResourceMetadataWriterImpl&) = delete;

  ~ServiceWorkerResourceMetadataWriterImpl() override;

  // mojom::ServiceWorkerResourceMetadataWriter implementations:
  void WriteMetadata(mojo_base::BigBuffer data,
                     WriteMetadataCallback callback) override;

 private:
  // Called while executing WriteMetadata().
  void ContinueWriteMetadata(mojo_base::BigBuffer data,
                             WriteMetadataCallback callback);
  void DidWriteMetadata(scoped_refptr<net::IOBuffer> buffer,
                        size_t write_amount,
                        int rv);

  DiskEntryOpener entry_opener_;

  // Stored as a data member to handle //net-style maybe-async methods.
  WriteMetadataCallback write_metadata_callback_;

#if DCHECK_IS_ON()
  enum class State {
    kIdle,
    kWriteMetadataStarted,
    kWriteMetadataHasEntry,
  };
  State state_ = State::kIdle;
#endif  // DCHECK_IS_ON()

  mojo::Receiver<mojom::ServiceWorkerResourceMetadataWriter> receiver_;

  base::WeakPtrFactory<ServiceWorkerResourceMetadataWriterImpl> weak_factory_{
      this};
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_OPS_H_
