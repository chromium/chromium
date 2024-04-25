// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/push_buffer_queue.h"

#include <atomic>

#include "base/notreached.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "third_party/protobuf/src/google/protobuf/util/delimited_message_util.h"

namespace chromecast {
namespace media {
namespace {

// The number of consecutive failed read attempts before the buffer is
// determined to be in an invalid state.
int kMaximumFailedReadAttempts = 10;

// The maximum size of a read/write window used by the underlying buffer. This
// is the maximum size of the array which will be cached for upcoming use.
// size_t type is used here to simplify comparison logic later on.
size_t kWindowSizeBytes = 32;

}  // namespace

// static
constexpr size_t PushBufferQueue::kBufferSizeBytes;

PushBufferQueue::PushBufferQueue()
    : producer_handler_(this),
      consumer_handler_(this),
      consumer_stream_(std::in_place, &consumer_handler_),
      protobuf_consumer_stream_(std::in_place, &consumer_stream_.value(), 1),
      producer_stream_(std::in_place, &producer_handler_) {
  DETACH_FROM_SEQUENCE(producer_sequence_checker_);
  DETACH_FROM_SEQUENCE(consumer_sequence_checker_);
}

PushBufferQueue::~PushBufferQueue() = default;

CmaBackend::BufferStatus PushBufferQueue::PushBuffer(
    const PushBufferRequest& request) {
  auto success = PushBufferImpl(request);
  if (success) {
    producer_handler_.ApplyNewBytesWritten();
  }

  return success ? CmaBackend::BufferStatus::kBufferSuccess
                 : CmaBackend::BufferStatus::kBufferFailed;
}

bool PushBufferQueue::PushBufferImpl(const PushBufferRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(producer_sequence_checker_);

  bytes_written_during_current_write_ = 0;

  // NOTE: This method is used instead of SerializeDelimitedToZeroCopyStream()
  // due to bugs in the method's implementation. See b/173477672.
  DCHECK(producer_stream_.has_value());
  bool success = google::protobuf::util::SerializeDelimitedToOstream(
      request, &producer_stream_.value());

  if (success) {
    producer_handler_.overflow();
  } else {
    // Now the stream is in a bad state, so recreate it. This should only occur
    // when the entire |buffer_| is full at time of writing.
    bytes_written_during_current_write_ = 0;
    producer_handler_.overflow();
    producer_stream_ = std::nullopt;
    producer_stream_.emplace(&producer_handler_);
  }

  return success;
}

bool PushBufferQueue::HasBufferedData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(consumer_sequence_checker_);
  return !is_in_invalid_state_ && GetAvailableyByteCount() != size_t{0};
}

std::optional<PushBufferQueue::PushBufferRequest>
PushBufferQueue::GetBufferedData() {
  auto result = GetBufferedDataImpl();
  if (result.has_value()) {
    consumer_handler_.ApplyNewBytesRead();
  }

  return result;
}

std::optional<PushBufferQueue::PushBufferRequest>
PushBufferQueue::GetBufferedDataImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(consumer_sequence_checker_);
  DCHECK(HasBufferedData());

  bytes_read_during_current_read_ = 0;
  PushBufferRequest request;

  DCHECK(protobuf_consumer_stream_.has_value());
  bool succeeded = google::protobuf::util::ParseDelimitedFromZeroCopyStream(
      &request, &protobuf_consumer_stream_.value(), nullptr /* clean_eof */);

  // This case will only occur in one of the following cases:
  // - Reading a PushBuffer at the same time it is being written.
  // - An error occurs while reading from the stream (specifically, a PushBuffer
  //   was serialized incorrectly).
  // The former case is not expected to occur, but is handled to be safe.
  // The latter case is only expected if the buffer is written to when not
  // enough space is available to handle the new write.
  if (!succeeded) {
    consecuitive_read_failures_++;
    if (+consecuitive_read_failures_ > kMaximumFailedReadAttempts) {
      // This means that data was probably serialized incorrectly.
      is_in_invalid_state_ = true;
    }

    // Reset the read pointers so that future reads re-read the old data.
    bytes_read_during_current_read_ = 0;
    consumer_handler_.ResetReadPointers();

    // If |!succeeded|, the streams have ended up in an unexpected state and
    // need to be recreated.
    protobuf_consumer_stream_ = std::nullopt;
    consumer_stream_ = std::nullopt;
    consumer_stream_.emplace(&consumer_handler_);
    protobuf_consumer_stream_.emplace(&consumer_stream_.value(), 1);

    return std::nullopt;
  }

  consecuitive_read_failures_ = 0;

  return request;
}

int PushBufferQueue::GetAvailableyByteCount() const {
  const int total_bytes_read =
      bytes_read_so_far_.load(std::memory_order_relaxed) +
      consumer_handler_.GetReadOffset();
  const int total_bytes_written =
      bytes_written_so_far_.load(std::memory_order_relaxed);
  return total_bytes_written - total_bytes_read;
}

PushBufferQueue::ProducerHandler::ProducerHandler(PushBufferQueue* queue)
    : queue_(queue) {
  DCHECK(queue_);
}

PushBufferQueue::ProducerHandler::~ProducerHandler() = default;

int PushBufferQueue::ProducerHandler::overflow(int ch) {
  // Get the number of bytes read and written so far.
  const size_t current_read_bytes =
      queue_->bytes_read_so_far_.load(std::memory_order_acquire);
  const int currently_written_bytes = UpdateBytesWritten();
  DCHECK_GE(static_cast<size_t>(currently_written_bytes), current_read_bytes);

  // Calculates the current size of the buffer.
  const size_t bytes_currently_used =
      currently_written_bytes - current_read_bytes;
  DCHECK_LE(bytes_currently_used, kBufferSizeBytes);

  // Calculates the number of bytes that should be included in the next write
  // window, which is the least of:
  // - |kWindowSizeBytes|
  // - The number that can be written before wrapping around to the beginning of
  //   the underlying array,
  // - The number of bytes available before the current read pointer.
  const size_t current_write_index = currently_written_bytes % kBufferSizeBytes;
  const size_t available_writable_bytes =
      std::min(kBufferSizeBytes - current_write_index,
               current_read_bytes + kBufferSizeBytes - currently_written_bytes);
  const size_t new_window_size =
      std::min(kWindowSizeBytes, available_writable_bytes);

  // If there is no writable area, then return a special value per method
  // contact.
  if (new_window_size == 0) {
    setp(epptr(), epptr());
    return std::char_traits<char>::eof();
  }

  // Update the pointers that determine the writable area and write the given
  // value |ch| if one was given.
  setp(&queue_->buffer_[current_write_index],
       &queue_->buffer_[current_write_index + new_window_size]);
  const bool should_write_ch = (ch != std::char_traits<char>::eof());
  if (should_write_ch) {
    sputc(static_cast<char>(ch));
  }
  return 1;  // This can be any value except std::char_traits<char>::eof().
}

void PushBufferQueue::ProducerHandler::ApplyNewBytesWritten() {
  queue_->bytes_written_so_far_.fetch_add(
      queue_->bytes_written_during_current_write_, std::memory_order_relaxed);
  queue_->bytes_written_during_current_write_ = 0;
}

size_t PushBufferQueue::ProducerHandler::UpdateBytesWritten() {
  const int change_in_write_count = pptr() - pbase();
  DCHECK_GE(change_in_write_count, 0);
  queue_->bytes_written_during_current_write_ += change_in_write_count;
  return queue_->bytes_written_so_far_.load(std::memory_order_relaxed) +
         queue_->bytes_written_during_current_write_;
}

PushBufferQueue::ConsumerHandler::ConsumerHandler(PushBufferQueue* queue)
    : queue_(queue) {
  DCHECK(queue_);
}

PushBufferQueue::ConsumerHandler::~ConsumerHandler() = default;

int PushBufferQueue::ConsumerHandler::underflow() {
  // Get the written and read bytes.
  const size_t currently_written_bytes =
      queue_->bytes_written_so_far_.load(std::memory_order_acquire);
  const size_t current_read_bytes = UpdateBytesRead();
  DCHECK_GE(currently_written_bytes, current_read_bytes);

  // Stop reading at either the end of the array or the current write index,
  // whichever is sooner. While there may be more data wrapped around after the
  // end of the array, that can be handled as part of the next underflow() call.
  const size_t avail = currently_written_bytes - current_read_bytes;
  const size_t begin = current_read_bytes % kBufferSizeBytes;
  const size_t end = std::min(begin + avail, kBufferSizeBytes);
  const size_t new_window_size = std::min(end - begin, kWindowSizeBytes);

  // This means that there are no bytes left to read. Return a special value per
  // method contract.
  if (new_window_size == 0) {
    return std::char_traits<char>::eof();
  }

  // Otherwise, there is still readable data. Update the readable window and
  // return the current character per method contact. Because
  // std::char_traits<char>::eof() is a special return code, cast to a uint to
  // avoid all negative results (EOF is guaranteed to be negative by the stl).
  DCHECK_LE(current_read_bytes + new_window_size, currently_written_bytes);
  setg(&queue_->buffer_[begin], &queue_->buffer_[begin],
       &queue_->buffer_[begin + new_window_size]);
  return static_cast<uint8_t>(queue_->buffer_[begin]);
}

void PushBufferQueue::ConsumerHandler::ResetReadPointers() {
  const size_t begin =
      queue_->bytes_read_so_far_.load(std::memory_order_relaxed) %
      kBufferSizeBytes;
  setg(&queue_->buffer_[begin], &queue_->buffer_[begin],
       &queue_->buffer_[begin]);
}

void PushBufferQueue::ConsumerHandler::ApplyNewBytesRead() {
  queue_->bytes_read_so_far_.fetch_add(queue_->bytes_read_during_current_read_,
                                       std::memory_order_relaxed);
  queue_->bytes_read_during_current_read_ = 0;
}

size_t PushBufferQueue::ConsumerHandler::UpdateBytesRead() {
  const int change_in_read_count = GetReadOffset();
  DCHECK_GE(change_in_read_count, 0);
  queue_->bytes_read_during_current_read_ += change_in_read_count;
  return queue_->bytes_read_so_far_.load(std::memory_order_relaxed) +
         queue_->bytes_read_during_current_read_;
}

int PushBufferQueue::ConsumerHandler::GetReadOffset() const {
  return gptr() - eback();
}

}  // namespace media
}  // namespace chromecast
