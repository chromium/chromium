// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_IO_BUFFER_POOL_H_
#define CHROMECAST_NET_IO_BUFFER_POOL_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"

namespace chromecast {

// Buffer pool to allocate ::net::IOBuffers of a constant size. When a buffer
// from this pool is destroyed, it is returned to the pool to be reused (rather
// than constantly reallocating memory). If the buffer is destroyed after the
// pool has been destroyed, the buffer's memory will be freed correctly.
class IOBufferPool : public base::RefCountedThreadSafe<IOBufferPool> {
 public:
  // |buffer_size| is the usable size of each buffer that will be returned
  // by GetBuffer(). |max_buffers| is the maximum number of buffers that this
  // pool will allocate. If |threadsafe| is true then the pool and any buffers
  // allocated from it may be used safely on any thread; otherwise all access
  // to pool and buffers must be made on the creating sequence.
  IOBufferPool(size_t buffer_size, size_t max_buffers, bool threadsafe = false);
  // If |max_buffers| is not specified, the maximum value of size_t is used.
  explicit IOBufferPool(size_t buffer_size);

  size_t buffer_size() const { return buffer_size_; }
  size_t max_buffers() const { return max_buffers_; }
  bool threadsafe() const { return threadsafe_; }

  // Ensures that at least |num_buffers| are allocated. If |num_buffers| is
  // greater than |max_buffers|, makes sure that |max_buffers| buffers have been
  // allocated.
  void Preallocate(size_t num_buffers);

  // Returns an IOBuffer from the pool, or |nullptr| if there are no buffers in
  // the pool and |max buffers| has been reached. The pool does not keep any
  // references to the buffer. It is safe to use the returned buffer after the
  // pool has been destroyed; if the buffer is destroyed after the pool is
  // destroyed, the buffer's memory will be freed correctly.
  scoped_refptr<net::IOBuffer> GetBuffer();

  // Returns the number of buffers that have ever been allocated by the pool.
  size_t NumAllocatedForTesting() const;
  // Returns the number of buffers that are currently free in the pool.
  size_t NumFreeForTesting() const;

 private:
  class Internal;

  friend class base::RefCountedThreadSafe<IOBufferPool>;
  ~IOBufferPool();

  const size_t buffer_size_;
  const size_t max_buffers_;
  const bool threadsafe_;
  Internal* internal_;  // Manages its own lifetime.

  DISALLOW_COPY_AND_ASSIGN(IOBufferPool);
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_IO_BUFFER_POOL_H_
