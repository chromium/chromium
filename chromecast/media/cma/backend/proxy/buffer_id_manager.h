// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_BUFFER_ID_MANAGER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_BUFFER_ID_MANAGER_H_

#include <memory>
#include <optional>
#include <queue>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/cma/backend/proxy/audio_decoder_pipeline_node.h"

namespace chromecast {
namespace media {

class CastDecoderBuffer;
class MonotonicClock;

// This class is responsible for assigning the IDs for PushBuffer commands sent
// over the CastRuntimeAudioChannel. Each PushBuffer command is expected to have
// an increasing BufferId (with the exception of wrap-around of large integers),
// to be used for audio synchronization between playback in the runtime and any
// further processing or work done on the other side of the
// MultizoneAudioDecoderProxy gRPC Channel.
//
// This class does NOT attempt to track the association between a given
// PushBuffer and its ID. Instead, it tracks the playback timestamp (PTS)
// associated with each frame. By determining the change between the PTS for any
// two adjacent frames, this class can keep a running tally of the total
// playback time that has been pushed to the underlying AudioDecoder's queue.
// Then the playing buffer ID can then be approximated by using the
// GetRenderingDelay() function  provided by  CmaBackend::AudioDecoder, which
// gives the total amount of time of playback data currently pending in the
// AudioDecoder's internal queue.
//
// Additionally, if the underlying decoder has a "large" change in rendering
// delay (meaning one that may be perceptible to the human ear) with respect to
// what is expected based on the above calculations, the client provided to the
// instance upon creation is used to inform the caller of this change.
class BufferIdManager {
 public:
  using BufferId = int64_t;

  // Timing information about a buffer to be targeted for playback changes.
  struct TargetBufferInfo {
    // The ID associated with the target buffer.
    BufferId buffer_id = 0;

    // The wall time in microseconds at which the target buffer is expected to
    // play, as returned by MonotonicClock::Now().
    int64_t timestamp_micros = 0;
  };

  // Observer responsible for informing the caller when unexpected buffer
  // related events occur.
  class Client {
   public:
    // Called when a timestamp update is needed, based on the processing rate of
    // buffered data.
    // NOTE: Pass-by-value is used here to save a copy through copy-elision.
    virtual void OnTimestampUpdateNeeded(TargetBufferInfo buffer) = 0;

   protected:
    virtual ~Client();
  };

  // |audio_decoder| and |client| are expected to remain valid for the lifetime
  // of this instance.
  BufferIdManager(CmaBackend::AudioDecoder* audio_decoder, Client* client);
  BufferIdManager(const BufferIdManager& other) = delete;
  BufferIdManager(BufferIdManager&& other) = delete;

  ~BufferIdManager();

  BufferIdManager& operator=(const BufferIdManager& other) = delete;
  BufferIdManager& operator=(BufferIdManager&& other) = delete;

  // Assigns the BufferId to use with the buffer for which it is called. This
  // value is used to keep a running tally of the expected pts at which a given
  // buffer will play, compared to when it is rendered.
  BufferId AssignBufferId(const CastDecoderBuffer& buffer);

  // Returns TargetBufferInfo associated with the APPROXIMATE BufferId
  // being rendered by the  underlying CmaBackend at this time. Note that,
  // although this is not exact, the human  ear cannot perceive variation on
  // the level introduced by this slight mismatch.
  TargetBufferInfo GetCurrentlyProcessingBufferInfo() const;

  // Uses the provided AudioDecoder to calculate approximately how many buffers
  // are remaining in the PushBuffer queue, removes all buffers expected to
  // have already played out. This ensures that |buffer_infos_| size does not
  // grow in an unbounded fashion. Returns TargetBufferInfo associated with the
  // APPROXIMATE BufferId being rendered by the  underlying CmaBackend at this
  // time, as above.
  TargetBufferInfo UpdateAndGetCurrentlyProcessingBufferInfo();

 private:
  friend class BufferIdManagerTest;

  // A wrapper around the buffer ID that acts for users like a queue.
  class BufferIdQueue {
   public:
    BufferIdQueue();

    BufferIdManager::BufferId Front() const;
    BufferIdManager::BufferId Pop();

   private:
    // The next id to use, initialized to a random number upon creation.
    // NOTE: A uint64_t is used rather than an int64_t to allow for buffer
    // overflow and wrap-around-to-zero without crashing the runtime.
    uint64_t next_id_;
  };

  // The information stored about each buffer upon an AssignBufferId() call.
  struct BufferInfo {
    // NOTE: Ctor added to support vector::emplace calls.
    BufferInfo(BufferId buffer_id, int64_t pts);

    // The ID of the current buffer.
    BufferId id = -1;

    // The PTS of the associated PushBuffer's data in microseconds, as defined
    // in DecoderBufferBase.
    int64_t pts_timestamp_micros = -1;
  };

  // The union of all information from both TargetBufferInfo and BufferInfo.
  // Used only to allow for more readable code when storing information about
  // the most recently played buffer.
  struct BufferPlayoutInfo {
    // The ID associated with the target buffer.
    BufferId id = -1;

    // The PTS of the associated PushBuffer's data in microseconds, as defined
    // in DecoderBufferBase.
    int64_t pts_timestamp_micros = -1;

    // The wall time in microseconds at which the target buffer is EXPECTED to
    // play, as understood by callers of this object's public methods.
    int64_t expected_playout_timestamp_micros = -1;
  };

  // To be used for testing.
  BufferIdManager(CmaBackend::AudioDecoder* audio_decoder,
                  Client* client,
                  std::unique_ptr<MonotonicClock> clock);

  // The total playback time of all buffers currently in |buffer_infos|.
  int64_t pending_playback_time_in_microseconds_ = 0;

  // Information about the most recently played buffer.
  mutable std::optional<BufferPlayoutInfo> most_recently_played_buffer_;

  BufferIdQueue buffer_id_queue_;
  std::queue<BufferInfo> buffer_infos_;
  std::unique_ptr<MonotonicClock> clock_;

  Client* const client_;
  CmaBackend::AudioDecoder* const audio_decoder_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<BufferIdManager> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_BUFFER_ID_MANAGER_H_
