// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/buffering_defs.h"

namespace chromecast {
namespace media {

// TODO(servolk): Find a way to compute those values dynamically, based on
// input stream parameters. These sizes need to allow enough data to be buffered
// to reach high memory threshold of the buffering controller (see
// kHighBufferThresholdMediaSource/kHighBufferThresholdURL being used in media
// pipeline initialization in MediaPipelineImpl::Initialize). Otherwise CMA IPC
// might deadlock (playback is kept paused by buffering_controller since we have
// less than |high_threshold| of data buffered, media DecoderBuffers are kept
// alive holding on to the IPC shared memory and CMA IPC is stuck since it
// reached the buffer limit and can't send more data to the browser process).
const size_t kAppAudioBufferSize = 256 * 1024;
const size_t kAppVideoBufferSize = 4 * 1024 * 1024;

}  // namespace media
}  // namespace chromecast
