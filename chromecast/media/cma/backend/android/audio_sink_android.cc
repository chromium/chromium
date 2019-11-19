// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/android/audio_sink_android.h"

#include "chromecast/media/cma/backend/android/audio_sink_android_audiotrack_impl.h"
#include "chromecast/media/cma/backend/android/audio_sink_manager.h"

namespace chromecast {
namespace media {

// static
const char* GetAudioContentTypeName(const AudioContentType type) {
  switch (type) {
    case AudioContentType::kMedia:
      return "kMedia";
    case AudioContentType::kAlarm:
      return "kAlarm";
    case AudioContentType::kCommunication:
      return "kCommunication";
    case AudioContentType::kOther:
      return "kOther";
    default:
      return "Unknown";
  }
}

// static
bool AudioSinkAndroid::GetSessionIds(SinkType sink_type,
                                     int* media_id,
                                     int* communication_id) {
  switch (sink_type) {
    case AudioSinkAndroid::kSinkTypeNativeBased:
      // TODO(ckuiper): implement a sink using native code.
      NOTREACHED() << "Native-based audio sink is not implemented yet!";
      break;
    case AudioSinkAndroid::kSinkTypeJavaBased:
      return AudioSinkAndroidAudioTrackImpl::GetSessionIds(media_id,
                                                           communication_id);
  }
  return false;
}

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
                             bool primary,
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
      sink_ = new AudioSinkAndroidAudioTrackImpl(delegate, num_channels,
                                                 samples_per_second, primary,
                                                 device_id, content_type);
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
