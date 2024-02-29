// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_PUSH_BUFFER_QUEUE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_PUSH_BUFFER_QUEUE_H_

#include <atomic>
#include <istream>
#include <optional>
#include <ostream>

#include "base/sequence_checker.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/cma/backend/proxy/audio_channel_push_buffer_handler.h"
#include "third_party/cast_core/public/src/proto/runtime/cast_audio_channel_service.pb.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl.h"

namespace chromecast {
namespace media {

struct AudioConfig;

// This class is responsible for buffering both DecoderBuffer and AudioConfig
// data, which are pushed together over gRPC using the PushData() API call.
// Two sequences are expected to simultaneously access this object:
// - A PRODUCER sequence, which will push new data in.
// - A CONSUMER sequence which will pull this data back out of the data
//   structure.
//
// This is achieved through serializing this protobuf into bytes, then storing
// these bytes in a lockless FIFO.
class PushBufferQueue : public AudioChannelPushBufferHandler {
 public:
  // The amount of space to allocate in the buffer.
  static constexpr size_t kBufferSizeBytes = 0x01 << 12;  // 4 kB.

  PushBufferQueue();
  PushBufferQueue(const PushBufferQueue& other) = delete;

  ~PushBufferQueue() override;

  PushBufferQueue& operator=(const PushBufferQueue& other) = delete;

  // AudioChannelPushBufferHandler overrides.
  CmaBackend::BufferStatus PushBuffer(
      const PushBufferRequest& request) override;
  bool HasBufferedData() const override;
  std::optional<PushBufferRequest> GetBufferedData() override;

 private:
  // These classes exist for the following 2 reasons:
  // 1) Readability. Separating of the Read and Write methods is simpler
  // 2) Thread safety guarantees. The stl provides no guarantees of thread
  //    safety within a single instance of std::basic_streambuf<char>, even
  //    though there should be no overlap between the resources used by both.
  // In an ideal world, this functionality could all live in the
  // |PushBufferQueue| class.
  //
  // The approach used by basic_streambuf is to maintain a 'window' on the data
  // from which it reads/writes. When the window runs out, underflow() or read
  // or overflow() for write is called to get the next window.
  // These allow this class to be used as a thread-safe circular read/write
  // buffer by input and output streams, as required for use with protobuf
  // serialization and deserialization utilities.
  //
  // Methods in |ProducerHandler| may only be called from the PRODUCER.
  class ProducerHandler : public std::basic_streambuf<char> {
   public:
    explicit ProducerHandler(PushBufferQueue* queue);
    ~ProducerHandler() override;

    // std::basic_streambuf<char> overrides:
    int overflow(int ch = std::char_traits<char>::eof()) override;

    // Stores the new value of |bytes_written_so_far_| following a successful
    // write.
    void ApplyNewBytesWritten();

   private:
    // Updates |bytes_written_during_current_write_| and returns the total
    // number of bytes written including these new bytes.
    size_t UpdateBytesWritten();

    PushBufferQueue* const queue_;
  };

  // Methods in |ConsumerHandler| may only be called from the CONSUMER.
  class ConsumerHandler : public std::basic_streambuf<char> {
   public:
    explicit ConsumerHandler(PushBufferQueue* queue);
    ~ConsumerHandler() override;

    // std::basic_streambuf<char> overrides:
    int underflow() override;

    // Returns the number of bytes that have been read so far but not accounted
    // for by |queue_->bytes_read_so_far_|.
    int GetReadOffset() const;

    // Resets the get area for this streambuf to start at the location pointed
    // to by |bytes_read_so_far_| and configures the stream to call underflow()
    // during its next read.
    void ResetReadPointers();

    // Stores the new value of |bytes_read_so_far_| following a successful read.
    void ApplyNewBytesRead();

   private:
    // Updates |bytes_read_during_current_read_| and returns the total number of
    // bytes read including these new bytes.
    size_t UpdateBytesRead();

    PushBufferQueue* const queue_;
  };

  // Friend declaration is needed to test some edge cases that can be hit when
  // simultaneous reads and writes are ongoing.
  friend class PushBufferQueueTests;

  // Give access to helper types.
  friend class ProducerHandler;
  friend class ConsumerHandler;

  // Gets the number of buffered bytes. May only be called from the CONSUMER.
  int GetAvailableyByteCount() const;

  // Helper methods to be used for test hooks.
  bool PushBufferImpl(const PushBufferRequest& request);
  std::optional<PushBufferRequest> GetBufferedDataImpl();

  // Buffer where serialized PushBufferRequest data is stored.
  char buffer_[kBufferSizeBytes];

  // Total number of bytes read or written by completed operations so far.
  // Atomics are used both to ensure that read and write operations are atomic
  // on all systems and to ensure that different values for these values aren't
  // loaded from each CPU's physical cache. Size_t types are used intentionally
  // to allow for wrap-around.
  std::atomic_size_t bytes_read_so_far_{0};
  std::atomic_size_t bytes_written_so_far_{0};

  // The number of bytes read during the current GetBufferedData() call. This is
  // necessary due to internal details of how an IstreamInputStream handles
  // end-of-stream conditions. May only be accessed or modified by the
  // CONSUMER.
  int bytes_read_during_current_read_ = 0;

  // The number of bytes written during the current PushBuffer call. This helps
  // to prevent reads of PushBuffer instances currently being written. May only
  // be accessed by the PRODUCER.
  int bytes_written_during_current_write_ = 0;

  // Tracks whether this buffer is in a valid state for further reads to occur.
  // May only be used by the CONSUMER.
  int consecuitive_read_failures_ = 0;
  bool is_in_invalid_state_ = false;

  // Helpers for keeping CONSUMER and PRODUCER sequences independent.
  ProducerHandler producer_handler_;
  ConsumerHandler consumer_handler_;

  // Sequence checkers for thread safety validation:
  SEQUENCE_CHECKER(producer_sequence_checker_);
  SEQUENCE_CHECKER(consumer_sequence_checker_);

  // Input streams backed by this instance. They must be optional so that they
  // can be re-created following a failed read. These should only be used by the
  // CONSUMER.
  std::optional<std::istream> consumer_stream_;
  std::optional<google::protobuf::io::IstreamInputStream>
      protobuf_consumer_stream_;

  // Output stream backed by this instance. This must be optional so it can be
  // re-created following a failed write. It should only be used by the
  // PRODUCER.
  std::optional<std::ostream> producer_stream_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_PUSH_BUFFER_QUEUE_H_
