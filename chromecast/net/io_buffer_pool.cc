// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/io_buffer_pool.h"

#include <new>

#include "base/bits.h"
#include "base/memory/aligned_memory.h"
#include "base/synchronization/lock.h"

namespace chromecast {

// The IOBufferPool allocates IOBuffers and the associated data as a single
// contiguous buffer. The buffer is laid out like this:
// |------------Wrapper----------|---data area---|
// |--IOBuffer--|--Internal ptr--|---data area---|
//
// The contiguous buffer is allocated as a character array, and then a Wrapper
// instance is placement-newed into it. We return a pointer to the IOBuffer
// within the Wrapper.
//
// When the IOBuffer is deleted (in operator delete), we get a pointer to the
// beginning of storage for the IOBuffer, which is the same memory location
// as the Wrapper instance (since the Wrapper has no vtable or base class, this
// should be true for any compiler). We can therefore cast the "deleted"
// pointer to a Wrapper* and then reclaim the buffer.
//
// All the actual data and logic for the buffer pool is held in the Internal
// class, which is refcounted with 1 ref for the IOBufferPool owner, and 1 ref
// for each buffer currently in use (ie, not in the free list). The Internal
// instance is only deleted when its internal refcount drops to 0; this allows
// buffers allocated from the pool to be safely used and deleted even after the
// pool has been destroyed.
//
// Note that locking in the Internal methods is optional since it is only needed
// when threadsafe operation is requested.

class IOBufferPool::Internal {
 public:
  Internal(size_t data_area_size, size_t max_buffers, bool threadsafe);

  Internal(const Internal&) = delete;
  Internal& operator=(const Internal&) = delete;

  size_t num_allocated() const {
    base::AutoLockMaybe lock(lock_ptr_);
    return num_allocated_;
  }

  size_t num_free() const {
    base::AutoLockMaybe lock(lock_ptr_);
    return num_free_;
  }

  void Preallocate(size_t num_buffers);

  void OwnerDestroyed();

  scoped_refptr<net::IOBuffer> GetBuffer();

 private:
  class Buffer;
  class Wrapper;
  union Storage;

  static constexpr size_t kAlignment = 16;

  static Storage* AllocateStorageUnionAndDataArea(size_t data_area_size);
  static char* DataAreaFromStorageUnion(Storage* ptr);

  ~Internal();

  void Reclaim(Wrapper* wrapper);

  const size_t data_area_size_;
  const size_t max_buffers_;

  mutable base::Lock lock_;
  base::Lock* const lock_ptr_;

  Storage* free_buffers_;
  size_t num_allocated_;
  size_t num_free_;

  int refs_;
};

class IOBufferPool::Internal::Buffer : public net::IOBuffer {
 public:
  Buffer(char* data, size_t size)
      : net::IOBuffer(base::make_span(data, size)) {}

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

 private:
  friend class Wrapper;

  ~Buffer() override = default;
  static void operator delete(void* ptr);
};

class IOBufferPool::Internal::Wrapper {
 public:
  Wrapper(char* data, size_t size, IOBufferPool::Internal* pool)
      : buffer_(data, size), pool_(pool) {}

  Wrapper(const Wrapper&) = delete;
  Wrapper& operator=(const Wrapper&) = delete;

  ~Wrapper() = delete;
  static void operator delete(void*) = delete;

  Buffer* buffer() { return &buffer_; }

  void Reclaim() { pool_->Reclaim(this); }

 private:
  Buffer buffer_;
  IOBufferPool::Internal* const pool_;
};

union IOBufferPool::Internal::Storage {
  Storage* next;  // Pointer to next free buffer.
  Wrapper wrapper;
};

void IOBufferPool::Internal::Buffer::operator delete(void* ptr) {
  Wrapper* wrapper = reinterpret_cast<Wrapper*>(ptr);
  wrapper->Reclaim();
}

IOBufferPool::Internal::Internal(size_t data_area_size,
                                 size_t max_buffers,
                                 bool threadsafe)
    : data_area_size_(data_area_size),
      max_buffers_(max_buffers),
      lock_ptr_(threadsafe ? &lock_ : nullptr),
      free_buffers_(nullptr),
      num_allocated_(0),
      num_free_(0),
      refs_(1) {  // 1 ref for the owner.
}

IOBufferPool::Internal::~Internal() {
  while (free_buffers_) {
    char* data = reinterpret_cast<char*>(free_buffers_);
    free_buffers_ = free_buffers_->next;
    base::AlignedFree(data);
  }
}

// Allocates aligned space for a `union Storage` plus an additional data
// area of `data_area_size` bytes with the same alignment.
// static
IOBufferPool::Internal::Storage*
IOBufferPool::Internal::AllocateStorageUnionAndDataArea(size_t data_area_size) {
  size_t kAlignedStorageSize = base::bits::AlignUp(sizeof(Storage), kAlignment);
  return reinterpret_cast<Storage*>(
      base::AlignedAlloc(kAlignedStorageSize + data_area_size, kAlignment));
}

// Returns a pointer to the data area that follows a `union Storage`.
// static
char* IOBufferPool::Internal::DataAreaFromStorageUnion(
    IOBufferPool::Internal::Storage* ptr) {
  size_t kAlignedStorageSize = base::bits::AlignUp(sizeof(Storage), kAlignment);
  return reinterpret_cast<char*>(ptr) + kAlignedStorageSize;
}

void IOBufferPool::Internal::Preallocate(size_t num_buffers) {
  // We assume that this is uncontended in normal usage, so just lock for the
  // entire method.
  base::AutoLockMaybe lock(lock_ptr_);
  if (num_buffers > max_buffers_) {
    num_buffers = max_buffers_;
  }
  if (num_allocated_ >= num_buffers) {
    return;
  }
  size_t num_extra_buffers = num_buffers - num_allocated_;
  num_free_ += num_extra_buffers;
  num_allocated_ += num_extra_buffers;
  while (num_extra_buffers > 0) {
    Storage* storage = AllocateStorageUnionAndDataArea(data_area_size_);
    storage->next = free_buffers_;
    free_buffers_ = storage;

    --num_extra_buffers;
  }
  // No need to add refs here, since the newly allocated buffers are not in use.
}

void IOBufferPool::Internal::OwnerDestroyed() {
  bool deletable;
  {
    base::AutoLockMaybe lock(lock_ptr_);
    --refs_;  // Remove the owner's ref.
    deletable = (refs_ == 0);
  }

  if (deletable) {
    delete this;
  }
}

scoped_refptr<net::IOBuffer> IOBufferPool::Internal::GetBuffer() {
  Storage* ptr = nullptr;

  {
    base::AutoLockMaybe lock(lock_ptr_);
    if (free_buffers_) {
      ptr = free_buffers_;
      free_buffers_ = free_buffers_->next;
      --num_free_;
    } else {
      if (num_allocated_ == max_buffers_)
        return nullptr;
      ++num_allocated_;
    }
    ++refs_;  // Add a ref for the now in-use buffer.
  }

  if (!ptr) {
    ptr = AllocateStorageUnionAndDataArea(data_area_size_);
  }

  char* data_area = DataAreaFromStorageUnion(ptr);
  Wrapper* wrapper =
      new (static_cast<void*>(ptr)) Wrapper(data_area, data_area_size_, this);
  return scoped_refptr<net::IOBuffer>(wrapper->buffer());
}

void IOBufferPool::Internal::Reclaim(Wrapper* wrapper) {
  Storage* storage = reinterpret_cast<Storage*>(wrapper);
  bool deletable;
  {
    base::AutoLockMaybe lock(lock_ptr_);
    storage->next = free_buffers_;
    free_buffers_ = storage;
    ++num_free_;
    --refs_;  // Remove a ref since this buffer is no longer in use.
    deletable = (refs_ == 0);
  }

  if (deletable) {
    delete this;
  }
}

IOBufferPool::IOBufferPool(size_t buffer_size,
                           size_t max_buffers,
                           bool threadsafe)
    : buffer_size_(buffer_size),
      max_buffers_(max_buffers),
      threadsafe_(threadsafe),
      internal_(new Internal(buffer_size_, max_buffers_, threadsafe_)) {}

IOBufferPool::IOBufferPool(size_t buffer_size)
    : IOBufferPool(buffer_size, static_cast<size_t>(-1)) {}

IOBufferPool::~IOBufferPool() {
  internal_->OwnerDestroyed();
}

size_t IOBufferPool::NumAllocatedForTesting() const {
  return internal_->num_allocated();
}

size_t IOBufferPool::NumFreeForTesting() const {
  return internal_->num_free();
}

void IOBufferPool::Preallocate(size_t num_buffers) {
  internal_->Preallocate(num_buffers);
}

scoped_refptr<net::IOBuffer> IOBufferPool::GetBuffer() {
  return internal_->GetBuffer();
}

}  // namespace chromecast
