// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_WAV_HEADER_H_
#define CHROMECAST_MEDIA_AUDIO_WAV_HEADER_H_

#include <stdint.h>

namespace chromecast {
namespace media {

// This is a header for a .wav file. It can be written directly to file (e.g.
// with reinterpret_cast<char*>) after SetDataSize(), SetNumChannels(), and
// SetSampleRate() have been called. It defaults to contain float-point data.
// From http://soundfile.sapp.org/doc/WaveFormat/ and
// http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
struct __attribute__((packed)) WavHeader {
  const char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t chunk_size;  // size of file - 8
  const char wave[4] = {'W', 'A', 'V', 'E'};
  const char fmt[4] = {'f', 'm', 't', ' '};
  const uint32_t subchunk_size = 18;
  uint16_t audio_format = 3;  // FLOAT
  uint16_t num_channels;
  uint32_t sample_rate;
  uint32_t byte_rate;    // sample_rate * num_channels * bytes per sample
  uint16_t block_align;  // num_channels * bytes per sample
  uint16_t bits_per_sample = 32;
  uint16_t extension_size = 0;
  const char data[4] = {'d', 'a', 't', 'a'};
  uint32_t subchunk_2_size;  // bytes in the data

  enum AudioFormat { kInteger8, kInteger16, kInteger32, kFloat32 };

  WavHeader();
  ~WavHeader() = default;

  void SetAudioFormat(AudioFormat audio_format_in) {
    switch (audio_format_in) {
      case kInteger8:
        audio_format = 1;
        bits_per_sample = 8;
        break;
      case kInteger16:
        audio_format = 1;
        bits_per_sample = 16;
        break;
      case kInteger32:
        audio_format = 1;
        bits_per_sample = 32;
        break;
      case kFloat32:
        audio_format = 3;
        bits_per_sample = 32;
        break;
    }
    byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    block_align = num_channels * bits_per_sample / 8;
  }

  void SetDataSize(int size_bytes) {
    chunk_size = 36 + size_bytes;
    subchunk_2_size = size_bytes;
  }

  void SetNumChannels(int num_channels_in) {
    num_channels = num_channels_in;
    byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    block_align = num_channels * bits_per_sample / 8;
  }

  void SetSampleRate(int sample_rate_in) {
    sample_rate = sample_rate_in;
    byte_rate = sample_rate * num_channels * bits_per_sample / 8;
  }
};

inline WavHeader::WavHeader() = default;

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_WAV_HEADER_H_
