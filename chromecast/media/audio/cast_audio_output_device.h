// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_DEVICE_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_DEVICE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "chromecast/common/mojom/audio_socket.mojom.h"
#include "media/base/audio_renderer_sink.h"
#include "media/mojo/mojom/cast_application_media_info_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromecast {
namespace media {

// Cast implementation of ::media::AudioRendererSink. |task_runner| passed in
// the constructor is the rendering sequence task runner. All methods internally
// run on the rendering sequence either directly or by posting a task.
// ::media::AudioRenderSink methods could be triggered on different threads.
class CastAudioOutputDevice : public ::media::AudioRendererSink {
 public:
  CastAudioOutputDevice(
      mojo::PendingRemote<mojom::AudioSocketBroker> audio_socket_broker,
      const std::string& session_id);

  CastAudioOutputDevice(const CastAudioOutputDevice&) = delete;
  CastAudioOutputDevice& operator=(const CastAudioOutputDevice&) = delete;

 private:
  // Only for testing.
  CastAudioOutputDevice(
      mojo::PendingRemote<mojom::AudioSocketBroker> audio_socket_broker,
      const std::string& session_id,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~CastAudioOutputDevice() override;

  class Internal;

  // ::media::AudioRendererSink implementation:
  void Initialize(const ::media::AudioParameters& params,
                  RenderCallback* callback) override;
  void Start() override;
  void Stop() override;
  void Pause() override;
  void Play() override;
  void Flush() override;
  bool SetVolume(double volume) override;
  ::media::OutputDeviceInfo GetOutputDeviceInfo() override;
  void GetOutputDeviceInfoAsync(OutputDeviceInfoCB info_cb) override;
  bool IsOptimizedForHardwareParameters() override;
  bool CurrentThreadIsRenderingThread() override;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::SequenceBound<std::unique_ptr<Internal>> internal_;
  Internal* internal_ptr_ = nullptr;

  mojo::PendingRemote<mojom::AudioSocketBroker> pending_socket_broker_;
  const std::string session_id_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_OUTPUT_DEVICE_H_
