// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_STREAM_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_STREAM_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/common/mojom/multiroom.mojom.h"
#include "chromecast/media/cma/backend/cma_backend.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromecast {
namespace media {

enum AudioOutputState {
  kClosed = 0,
  kOpened = 1,
  kStarted = 2,
  kPendingClose = 3,
};

class CastAudioManager;
class MixerServiceConnectionFactory;

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
// MixerServiceWrapper creates a new MixerServiceConnection at Start() and
// destroys the MixerServiceConnection at Stop(). When the volume is adjusted
// between a Stop() and the next Start(), the volume is recorded and then
// applied to MixerServiceConnection after the MixerServiceConnection is
// established on the Start() call.

// TODO(b/117980762): CastAudioOutputStream should be refactored
// to be more unit test friendly. And the unit tests can be improved.

class CastAudioOutputStream : public ::media::AudioOutputStream {
 public:
  // When nullptr is passed as |mixer_service_connection_factory|, CmaWrapper
  // will be used for audio playback.
  CastAudioOutputStream(
      CastAudioManager* audio_manager,
      service_manager::Connector* connector,
      const ::media::AudioParameters& audio_params,
      MixerServiceConnectionFactory* mixer_service_connection_factory);
  ~CastAudioOutputStream() override;

  // ::media::AudioOutputStream implementation.
  bool Open() override;
  void Close() override;
  void Start(AudioSourceCallback* source_callback) override;
  void Stop() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;

 private:
  class CmaWrapper;
  class MixerServiceWrapper;

  void FinishClose();
  void OnGetMultiroomInfo(const std::string& application_session_id,
                          chromecast::mojom::MultiroomInfoPtr multiroom_info);
  void InitializeCmaBackend(const std::string& application_session_id,
                            chromecast::mojom::MultiroomInfoPtr multiroom_info);

  double volume_;
  AudioOutputState audio_thread_state_;
  CastAudioManager* const audio_manager_;
  service_manager::Connector* connector_;
  const ::media::AudioParameters audio_params_;
  MixerServiceConnectionFactory* mixer_service_connection_factory_;
  chromecast::mojom::MultiroomManagerPtr multiroom_manager_;
  std::unique_ptr<CmaWrapper> cma_wrapper_;
  std::unique_ptr<MixerServiceWrapper> mixer_service_wrapper_;

  // Hold bindings to Start and SetVolume if they were called before Open
  // completed. After initialization has finished, these bindings will be
  // called.
  base::OnceCallback<void()> pending_start_;
  base::OnceCallback<void()> pending_volume_;

  THREAD_CHECKER(audio_thread_checker_);
  base::WeakPtrFactory<CastAudioOutputStream> audio_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastAudioOutputStream);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_STREAM_H_
