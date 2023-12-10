// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_STREAM_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_STREAM_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chromecast/base/task_runner_impl.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"

namespace chromecast {
namespace media {

class CmaAudioOutputStream;
class CastAudioManagerHelper;

// Chromecast implementation of AudioOutputStream.
// This class forwards to MixerService if valid
// |mixer_service_connection_factory| is passed in on the construction call,
// for a lower latency audio playback (using MixerServiceWrapper). Otherwise,
// when a nullptr is passed in as |mixer_service_connection_factory| it forwards
// to CMA backend (using CmaWrapper).
//
// In either case, involved components live on two threads:
// 1. Audio thread
//    |CastAudioOutputStream|
//    Where the object gets construction from AudioManager.
//    How the object gets controlled from AudioManager.
// 2. Media thread or an IO thread opened within |AudioOutputStream|.
//    |CastAudioOutputStream::CmaWrapper| or |MixerServiceWrapper| lives on
//    this thread.
//
// The interface between AudioManager and AudioOutputStream is synchronous, so
// in order to allow asynchronous thread hops, we:
// * Maintain the current state independently in each thread.
// * Cache function calls like Start() and SetVolume() as bound callbacks.
//
// The individual thread states should nearly always be the same. The only time
// they are expected to be different is when the audio thread has executed a
// task and posted to the media thread/IO thread, but the media thread has not
// executed yet.
//
// The below illustrates the case when CMA backend is used for playback.
//
//  Audio Thread |CAOS|                         Media Thread |CmaWrapper|
//  ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯                         ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//  *[ Closed ]                                 *[ Closed ]
//      |                                           |
//      |                                           |
//      v                                           v
//   [   Opened    ]    --post open-->           [   Opened    ]
//      |     |  ^                                  |     |  ^
//      |     |  |                                  |     |  |
//      |     | Stop()  --post stop-->              |     | Stop()
//      |     v  |                                  |     v  |
//      |  [ Started ]  --post start-->             |  [ Started ]
//      |     |                                     |     |
//      |     |                                     |     |
//      v     v                                     v     v
// **[ Pending Close ]  --post close-->        **[ Pending Close ]
//      |                                           |
//      |                                           |
//   ( waits for closure )         <--post closure--'
//      |
//      v
//   ( released)
// *  Initial states.
// ** Final states.
//
// When MixerService is used in place of CMA backend, the state transition is
// similar but a little simpler.
// MixerServiceWrapper creates a new mixer service connection at Start() and
// destroys the connection at Stop(). When the volume is adjusted between a
// Stop() and the next Start(), the volume is recorded and then applied to the
// mixer service connection after the connection is established on the Start()
// call.

// TODO(b/117980762): CastAudioOutputStream should be refactored
// to be more unit test friendly. And the unit tests can be improved.

class CastAudioOutputStream : public ::media::AudioOutputStream {
 public:
  // When nullptr is passed as |mixer_service_connection_factory|, CmaWrapper
  // will be used for audio playback.
  // |device_id_or_group_id| describes either the |device_id_| or |group_id_|.
  // If the |device_id_or_group_id| matches a valid device_id then the
  // |group_id_| is filled in empty. If the |device_id_or_group_id_| does not
  // match a valid |device_id|, then the |group_id_| is set to that value, and
  // the |device_id_| is set to kDefaultDeviceId. Valid device_id's are either
  // ::media::AudioDeviceDescription::kDefaultDeviceId or
  // ::media::AudioDeviceDescription::kCommunicationsId.
  CastAudioOutputStream(CastAudioManagerHelper* audio_manager,
                        const ::media::AudioParameters& audio_params,
                        const std::string& device_id_or_group_id,
                        bool use_mixer_service);

  CastAudioOutputStream(const CastAudioOutputStream&) = delete;
  CastAudioOutputStream& operator=(const CastAudioOutputStream&) = delete;

  ~CastAudioOutputStream() override;

  // ::media::AudioOutputStream implementation.
  bool Open() override;
  void Close() override;
  void Start(AudioSourceCallback* source_callback) override;
  void Stop() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;
  void Flush() override;

 private:
  enum class AudioOutputState {
    kClosed,
    kOpened,
    kStarted,
    kPendingClose,
  };

  class MixerServiceWrapper;

  void FinishClose();

  double volume_;
  AudioOutputState audio_thread_state_;
  CastAudioManagerHelper* const audio_manager_;
  const ::media::AudioParameters audio_params_;
  // Valid |device_id_| are kDefaultDeviceId, and kCommunicationsDeviceId
  const std::string device_id_;
  // |group_id_|s are uuids mapped to session_ids for multizone. Should be an
  // empty string if group_id is unused.
  const std::string group_id_;
  const bool use_mixer_service_;
  std::unique_ptr<CmaAudioOutputStream> cma_wrapper_;
  std::unique_ptr<MixerServiceWrapper> mixer_service_wrapper_;

  THREAD_CHECKER(audio_thread_checker_);
  base::WeakPtr<CastAudioOutputStream> audio_weak_this_;
  base::WeakPtrFactory<CastAudioOutputStream> audio_weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_STREAM_H_
