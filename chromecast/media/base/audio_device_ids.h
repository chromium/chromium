// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_AUDIO_DEVICE_IDS_H_
#define CHROMECAST_MEDIA_BASE_AUDIO_DEVICE_IDS_H_

namespace chromecast {
namespace media {

// Local-only audio (don't send over multiroom).
extern const char kLocalAudioDeviceId[];

extern const char kPlatformAudioDeviceId[];

extern const char kAlarmAudioDeviceId[];

extern const char kNoDelayDeviceId[];

// TODO(kmackay|bshaya) Remove this, just use
// ::media::AudioDeviceDescription::kCommunicationsDeviceId.
extern const char kTtsAudioDeviceId[];

// Local-only audio (don't send over multiroom) that should not be affected by
// post-processors.
extern const char kBypassAudioDeviceId[];

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_AUDIO_DEVICE_IDS_H_
