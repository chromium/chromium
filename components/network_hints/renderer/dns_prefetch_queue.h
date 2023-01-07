// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DnsQueue is implemented as an almost FIFO circular buffer for text
// strings that don't have embedded nulls ('\0').  The "almost" element is that
// some duplicate strings may be removed (i.e., the string won't really be
// pushed *if* the class happens to notice that a duplicate is already in the
// queue).
// The buffers internal format is null terminated character strings
// (a.k.a., c_strings).
// It is written to be as fast as possible during push() operations, so
// that there will be minimal performance impact on a supplier thread.
// The push() operation will not block, and no memory allocation is involved
// (internally) during the push operations.
// The one caveat is that if there is insufficient space in the buffer to
// accept additional string via a push(), then the push() will fail, and
// the buffer will be unmodified.

// This class was designed for use in DNS prefetch operations.  During
// rendering, the supplier is the renderer (typically), and the consumer
// is a thread that sends messages to an async DNS resolver.

#ifndef COMPONENTS_NETWORK_HINTS_RENDERER_DNS_PREFETCH_QUEUE_H__
#define COMPONENTS_NETWORK_HINTS_RENDERER_DNS_PREFETCH_QUEUE_H__

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

namespace network_hints {

// A queue of DNS lookup requests for internal use within the network_hints
// component.
class DnsQueue {
 public:
  // BufferSize is a signed type used for indexing into a buffer.
  typedef int32_t BufferSize;

  enum PushResult { SUCCESSFUL_PUSH, OVERFLOW_PUSH, REDUNDANT_PUSH };

  // The size specified in the constructor creates a buffer large enough
  // to hold at most one string of that length, or "many"
  // strings of considerably shorter length.  Note that strings
  // are padded internally with a terminal '\0" while stored,
  // so if you are trying to be precise and get N strings of
  // length K to fit, you should actually construct a buffer with
  // an internal size of N*(K+1).
  explicit DnsQueue(BufferSize size);

  DnsQueue(const DnsQueue&) = delete;
  DnsQueue& operator=(const DnsQueue&) = delete;

  ~DnsQueue(void);

  size_t Size() const { return size_; }
  void Clear();

  // Push takes an unterminated string of the given length
  // and inserts it into the queue for later
  // extraction by read.  For each successful push(), there
  // can later be a corresponding read() to extracted the text.
  // The string must not contain an embedded null terminator
  // Exactly length chars are written, or the push fails (where
  // "fails" means nothing is written).
  // Returns true for success, false for failure (nothing written).
  PushResult Push(const char* source, const size_t length);

  PushResult Push(const std::string& source) {
    return Push(source.c_str(), source.length());
  }

  // Extract the next available string from the buffer.
  // If the buffer is empty, then return false.
  bool Pop(std::string* out_string);

 private:
  bool Validate();  // Checks that all internal data is valid.

  // Circular buffer, plus extra char ('\0').
  const std::unique_ptr<char[]> buffer_;
  const BufferSize buffer_size_;  // Size one smaller than allocated space.
  const BufferSize buffer_sentinel_;  // Index of extra '\0' at end of buffer_.

  // If writable_ == readable_, then the buffer is empty.
  BufferSize readable_;  // Next readable char in buffer_.
  BufferSize writeable_;  // The next space in buffer_ to push.

  // Number of queued strings
  size_t size_;
};  // class DnsQueue

}  // namespace network_hints

#endif  // COMPONENTS_NETWORK_HINTS_RENDERER_DNS_PREFETCH_QUEUE_H__
