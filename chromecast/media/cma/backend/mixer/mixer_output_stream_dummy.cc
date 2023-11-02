// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/mixer_output_stream_dummy.h"

namespace chromecast {
namespace media {

MixerOutputStreamDummy::MixerOutputStreamDummy() = default;

MixerOutputStreamDummy::~MixerOutputStreamDummy() = default;

bool MixerOutputStreamDummy::Start(int requested_sample_rate, int channels) {
  return true;
}

int MixerOutputStreamDummy::GetNumChannels() {
  return 2;
}

int MixerOutputStreamDummy::GetSampleRate() {
  return 48000;
}

MediaPipelineBackend::AudioDecoder::RenderingDelay
MixerOutputStreamDummy::GetRenderingDelay() {
  return MediaPipelineBackend::AudioDecoder::RenderingDelay();
}

int MixerOutputStreamDummy::OptimalWriteFramesCount() {
  return 256;
}

bool MixerOutputStreamDummy::Write(const float* data,
                                   int data_size,
                                   bool* out_playback_interrupted) {
  return true;
}

void MixerOutputStreamDummy::Stop() {}

// static
std::unique_ptr<MixerOutputStream> MixerOutputStream::Create() {
  return std::make_unique<MixerOutputStreamDummy>();
}

}  // namespace media
}  // namespace chromecast
