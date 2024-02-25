// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_MEDIA_PIPELINE_DEVICE_PARAMS_H_
#define CHROMECAST_PUBLIC_MEDIA_MEDIA_PIPELINE_DEVICE_PARAMS_H_

#include <stdint.h>

#include <ostream>
#include <string>

#include "chromecast/public/volume_control.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromecast {
class TaskRunner;

namespace media {

enum class AudioChannel {
  kAll = 0,
  kLeft = 1,
  kRight = 2,
};

// Supplies creation parameters to platform-specific pipeline backend.
struct MediaPipelineDeviceParams {
  enum MediaSyncType {
    // Default operation, synchronize playback using PTS with higher latency.
    kModeSyncPts = 0,
    // With this mode, synchronization is disabled and audio/video frames are
    // rendered "right away":
    // - for audio, frames are still rendered based on the sampling frequency
    // - for video, frames are rendered as soon as available at the output of
    //   the video decoder.
    //   The assumption is that no B frames are used when synchronization is
    //   disabled, otherwise B frames would always be skipped.
    kModeIgnorePts = 1,
    // In addition to the constraints above, also do not wait for vsync.
    kModeIgnorePtsAndVSync = 2,
    // Almost same as kModeSyncPts except two things:
    // 1. When pushing silence to the backend decoder, set an invalid timestamp
    // to the silence buffer. We always play the silence, but if the stream uses
    // hardware av sync mode, the invalid timestamp will not be used to do av
    // sync.
    // 2. When pushing non-silence buffers, do not adjust the timestamp. When
    // calculating the rendering delay, the silence buffer will be counted. But
    // when calculating the current playback position of the real audio data,
    // buffers without timestamp, like silence buffer, will not be counted.
    kModeApkSyncPts = 3,
  };

  enum AudioStreamType {
    // "Real" audio stream. If this stream underruns, all audio output may pause
    // until more real stream data is available.
    kAudioStreamNormal = 0,
    // Sound-effects audio stream. May be interrupted if a real audio stream
    // is created with a different sample rate. Underruns on an effects stream
    // do not affect output of real audio streams.
    kAudioStreamSoundEffects = 1,
  };

  MediaPipelineDeviceParams();

  // TODO(guohuideng): Get rid of these excessive number of constructors, using
  // default arguments.
  MediaPipelineDeviceParams(TaskRunner* task_runner_in,
                            AudioContentType content_type_in,
                            const std::string& device_id_in);

  MediaPipelineDeviceParams(MediaSyncType sync_type_in,
                            TaskRunner* task_runner_in,
                            AudioContentType content_type_in,
                            const std::string& device_id_in);

  MediaPipelineDeviceParams(MediaSyncType sync_type_in,
                            AudioStreamType audio_type_in,
                            TaskRunner* task_runner_in,
                            AudioContentType content_type_in,
                            const std::string& device_id_in);

  MediaPipelineDeviceParams(const MediaPipelineDeviceParams& other);
  MediaPipelineDeviceParams& operator=(const MediaPipelineDeviceParams& other);
  MediaPipelineDeviceParams(MediaPipelineDeviceParams&& other);
  MediaPipelineDeviceParams& operator=(MediaPipelineDeviceParams&& other);

  MediaSyncType sync_type = kModeSyncPts;
  AudioStreamType audio_type = kAudioStreamNormal;

  // This flag matters only when the implementation of
  // CastMediaShlib::CreateMediaPipelineBackend(...) can return multiple kinds
  // of CastMediaShlib::MediaPipelineBackend. When this flag is true,
  // CastMediaShlib::CreateMediaPipelineBackend(...) should return a backend
  // that supports pass-through audio if it is possible.
  bool pass_through_audio_support_desired = false;

  // task_runner allows backend implementations to post tasks to the media
  // thread.  Since all calls from cast_shell into the backend are made on
  // the media thread, this may simplify thread management and safety for
  // some backends.
  TaskRunner* task_runner = nullptr;

  // This field is deprecated.
  //
  // TODO(yucliu): Remove this field.
  service_manager::Connector* connector = nullptr;

  // Identifies the content type for volume control.
  AudioContentType content_type = AudioContentType::kMedia;
  std::string device_id;

  // ID of the current session.
  std::string session_id;

  // This field is deprecated and ignored.
  bool multiroom = false;

  // Audio channel this device is playing.
  AudioChannel audio_channel = AudioChannel::kAll;

  // This field is deprecated and ignored.
  int64_t output_delay_us = 0;
};

inline MediaPipelineDeviceParams::MediaPipelineDeviceParams() = default;

inline MediaPipelineDeviceParams::MediaPipelineDeviceParams(
    TaskRunner* task_runner_in,
    AudioContentType content_type_in,
    const std::string& device_id_in)
    : task_runner(task_runner_in),
      content_type(content_type_in),
      device_id(device_id_in) {}

inline MediaPipelineDeviceParams::MediaPipelineDeviceParams(
    MediaSyncType sync_type_in,
    TaskRunner* task_runner_in,
    AudioContentType content_type_in,
    const std::string& device_id_in)
    : sync_type(sync_type_in),
      task_runner(task_runner_in),
      content_type(content_type_in),
      device_id(device_id_in) {}

inline MediaPipelineDeviceParams::MediaPipelineDeviceParams(
    MediaSyncType sync_type_in,
    AudioStreamType audio_type_in,
    TaskRunner* task_runner_in,
    AudioContentType content_type_in,
    const std::string& device_id_in)
    : sync_type(sync_type_in),
      audio_type(audio_type_in),
      task_runner(task_runner_in),
      content_type(content_type_in),
      device_id(device_id_in) {}

inline MediaPipelineDeviceParams::MediaPipelineDeviceParams(
    const MediaPipelineDeviceParams& other) = default;

inline MediaPipelineDeviceParams& MediaPipelineDeviceParams::operator=(
    const MediaPipelineDeviceParams& other) = default;

inline MediaPipelineDeviceParams::MediaPipelineDeviceParams(
    MediaPipelineDeviceParams&& other) = default;

inline MediaPipelineDeviceParams& MediaPipelineDeviceParams::operator=(
    MediaPipelineDeviceParams&& other) = default;

inline std::ostream& operator<<(std::ostream& os, AudioChannel audio_channel) {
  switch (audio_channel) {
    case AudioChannel::kAll:
      os << "all";
      return os;
    case AudioChannel::kLeft:
      os << "left";
      return os;
    case AudioChannel::kRight:
      os << "right";
      return os;
  }
  os << "unknown";
  return os;
}

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_MEDIA_PIPELINE_DEVICE_PARAMS_H_
