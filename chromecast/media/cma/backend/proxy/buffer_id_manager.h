// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_BUFFER_ID_MANAGER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_BUFFER_ID_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <queue>

#include "base/sequence_checker.h"
#include "chromecast/media/api/cma_backend.h"

namespace chromecast {
namespace media {

struct AudioConfig;

// This class is responsible for assigning the IDs for PushBuffer commands sent
// over the CastRuntimeAudioChannel. Each PushBuffer command is expected to have
// an increasing BufferId (with the exception of wrap-around of large integers),
// to be used for audio synchronization between playback in the runtime and any
// further processing or work done on the other side of the gRPC Channel.
//
// This class does NOT attempt to track the association between a given
// PushBuffer and its ID. Instead, it tracks the number of bytes of playback
// data associated with each frame. By calculating the number of bytes
// associated with one microsecond of PushBuffer data, the currently playing
// buffer ID can then be calculated by using the GetRenderingDelay() function
// provided by a CmaBackend::AudioDecoder, which gives the total amount of time
// of playback data currently pending in the AudioDecoder's internal queue.
//
// Specifically, if this calculation yields that X bytes of data are currently
// pending in the AudioDecoder's queue, this means that the buffer which is
// followed by (approximately) X bytes of data is (probably). While this
// algorithm does not yield an exact result, the human ear cannot differentiate
// the amount of delay this approximation can introduce.
class BufferIdManager {
 public:
  using BufferId = int64_t;

  // |audio_decoder| is expected to remain valid for the lifetime of this
  // instance.
  explicit BufferIdManager(CmaBackend::AudioDecoder* audio_decoder);
  ~BufferIdManager();

  // Sets the audio config to use for future buffer pushes. This |config| is
  // used to calculate the number of bytes of data per microsecond of audio
  // playback.
  void SetAudioConfig(const AudioConfig& audio_config);

  // Assigns the BufferId to use with the buffer for which it is called. This
  // value is used to keep a running tally of the number of bytes of audio
  // data being processed, as described in the class-level documentation. May
  // not be called prior to the first call to SetAudioConfig().
  BufferId AssignBufferId(size_t buffer_size_in_bytes);

  // Returns the APPROXIMATE BufferId which is currently being rendered by the
  // underlying CmaBackend. Note that, although this is not exact, the human
  // ear cannot perceive variation  on the level introduced by this slight
  // mismatch.
  BufferId GetCurrentlyProcessingBuffer();

 private:
  // A wrapper around the buffer ID that acts for users like a queue.
  class BufferIdQueue {
   public:
    BufferIdQueue();

    BufferIdManager::BufferId Front();
    BufferIdManager::BufferId Pop();

   private:
    // The next id to use, initialized to a random number upon creation.
    // NOTE: A uint64_t is used rather than an int64_t to allow for buffer
    // overflow and wrap-around-to-zero without crashing the runtime.
    uint64_t next_id_;
  };

  struct BufferInfo {
    // NOTE: Ctor added to support vector::emplace calls.
    BufferInfo(BufferId buffer_id, double buffer_playback_time_in_microseconds);

    // The ID of the current buffer.
    BufferId id = -1;

    // The duration of time in microseconds of playback which will be rendered
    // by the CmaBackend when the associated buffer is processed (as number of
    // bytes of data in this buffer divided by the number of bytes-per-second of
    // audio playback). This must be tracked rather than the number of bytes in
    // the source buffer to allow for changes to the audio config part way
    // through playback.
    double playback_time_in_microseconds = -1;
  };

  // Uses the provided AudioDecoder to calculate approximately how many buffers
  // are remaining in the PushBuffer queue, removes all buffers expected to
  // have already played out. This ensures that |buffer_infos_| size does not
  // grow in an unbounded fashion.
  void PruneBufferInfos();

  // The total playback time of all buffers currently in |buffer_infos|.
  double pending_playback_time_in_microseconds_ = 0;

  // Number of bytes per second of media playback. This will change each time
  // that SetAudioConfig() is called.
  double bytes_per_microsecond_ = 0;

  BufferIdQueue buffer_id_queue_;
  std::queue<BufferInfo> buffer_infos_;
  CmaBackend::AudioDecoder* const audio_decoder_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<BufferIdManager> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_BUFFER_ID_MANAGER_H_
