// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/android/audio_sink_android.h"

#include "base/logging.h"
#include "chromecast/media/cma/backend/android/audio_sink_android_audiotrack_impl.h"
#include "chromecast/media/cma/backend/android/audio_sink_manager.h"

namespace chromecast {
namespace media {

// static
int64_t AudioSinkAndroid::GetMinimumBufferedTime(SinkType sink_type,
                                                 const AudioConfig& config) {
  const int64_t kDefaultMinBufferTimeUs = 50000;
  switch (sink_type) {
    case AudioSinkAndroid::kSinkTypeNativeBased:
      // TODO(ckuiper): implement a sink using native code.
      NOTREACHED() << "Native-based audio sink is not implemented yet!";
      break;
    case AudioSinkAndroid::kSinkTypeJavaBased:
      return AudioSinkAndroidAudioTrackImpl::GetMinimumBufferedTime(
          config.channel_number, config.samples_per_second);
  }
  return kDefaultMinBufferTimeUs;
}

ManagedAudioSink::ManagedAudioSink(SinkType sink_type)
    : sink_type_(sink_type), sink_(nullptr) {}

ManagedAudioSink::~ManagedAudioSink() {
  Remove();
}

void ManagedAudioSink::Reset() {
  Remove();
}

void ManagedAudioSink::Reset(Delegate* delegate,
                             int num_channels,
                             int samples_per_second,
                             int audio_track_session_id,
                             bool primary,
                             bool use_hw_av_sync,
                             const std::string& device_id,
                             AudioContentType content_type) {
  Remove();

  LOG(INFO) << __func__ << ": Creating new sink of type=" << sink_type_;
  switch (sink_type_) {
    case AudioSinkAndroid::kSinkTypeNativeBased:
      // TODO(ckuiper): implement a sink using native code.
      NOTREACHED() << "Native-based audio sink is not implemented yet!";
      break;
    case AudioSinkAndroid::kSinkTypeJavaBased:
      sink_ = new AudioSinkAndroidAudioTrackImpl(
          delegate, num_channels, samples_per_second, audio_track_session_id,
          primary, use_hw_av_sync, device_id, content_type);
  }
  AudioSinkManager::Get()->Add(sink_);
}

void ManagedAudioSink::Remove() {
  if (sink_) {
    AudioSinkManager::Get()->Remove(sink_);
    delete sink_;
    sink_ = nullptr;
  }
}

}  // namespace media
}  // namespace chromecast
