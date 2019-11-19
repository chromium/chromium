// Copyright 2019 The Chromium Authors. All rights reserved.
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
// |------------Wrapper----------|---data buffer---|
// |--IOBuffer--|--Internal ptr--|---data buffer---|
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
  Internal(size_t buffer_size, size_t max_buffers, bool threadsafe);

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

  static void* AllocateAlignedSpace(size_t buffer_size);

  ~Internal();

  void Reclaim(Wrapper* wrapper);

  const size_t buffer_size_;
  const size_t max_buffers_;

  mutable base::Lock lock_;
  base::Lock* const lock_ptr_;

  Storage* free_buffers_;
  size_t num_allocated_;
  size_t num_free_;

  int refs_;

  DISALLOW_COPY_AND_ASSIGN(Internal);
};

class IOBufferPool::Internal::Buffer : public net::IOBuffer {
 public:
  explicit Buffer(char* data) : net::IOBuffer(data) {}

 private:
  friend class Wrapper;

  ~Buffer() override { data_ = nullptr; }
  static void operator delete(void* ptr);

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

class IOBufferPool::Internal::Wrapper {
 public:
  Wrapper(char* data, IOBufferPool::Internal* pool)
      : buffer_(data), pool_(pool) {}

  ~Wrapper() = delete;
  static void operator delete(void*) = delete;

  Buffer* buffer() { return &buffer_; }

  void Reclaim() { pool_->Reclaim(this); }

 private:
  Buffer buffer_;
  IOBufferPool::Internal* const pool_;

  DISALLOW_COPY_AND_ASSIGN(Wrapper);
};

union IOBufferPool::Internal::Storage {
  Storage* next;  // Pointer to next free buffer.
  Wrapper wrapper;
};

void IOBufferPool::Internal::Buffer::operator delete(void* ptr) {
  Wrapper* wrapper = reinterpret_cast<Wrapper*>(ptr);
  wrapper->Reclaim();
}

IOBufferPool::Internal::Internal(size_t buffer_size,
                                 size_t max_buffers,
                                 bool threadsafe)
    : buffer_size_(buffer_size),
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

// static
void* IOBufferPool::Internal::AllocateAlignedSpace(size_t buffer_size) {
  size_t kAlignedStorageSize = base::bits::Align(sizeof(Storage), kAlignment);
  return base::AlignedAlloc(kAlignedStorageSize + buffer_size, kAlignment);
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
    void* ptr = AllocateAlignedSpace(buffer_size_);
    Storage* storage = reinterpret_cast<Storage*>(ptr);
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
  char* ptr = nullptr;

  {
    base::AutoLockMaybe lock(lock_ptr_);
    if (free_buffers_) {
      ptr = reinterpret_cast<char*>(free_buffers_);
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
    ptr = static_cast<char*>(AllocateAlignedSpace(buffer_size_));
  }

  size_t kAlignedStorageSize = base::bits::Align(sizeof(Storage), kAlignment);
  char* data = ptr + kAlignedStorageSize;
  Wrapper* wrapper = new (ptr) Wrapper(data, this);
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
      internal_(new Internal(buffer_size, max_buffers, threadsafe)) {}

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
