// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_PACKET_HEADER_H_
#define CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_PACKET_HEADER_H_

#include <cstdint>

namespace chromecast {
namespace media {
namespace capture_service {

// Memory block of a packet header. Changes to it need to make sure about the
// memory alignment to avoid extra paddings being inserted. It reflects real
// packet header structure, however, the |size| bits are in big-endian order,
// and thus is only for padding purpose in this struct, when all bytes after it
// represent a message header.
struct __attribute__((__packed__)) PacketHeader {
  uint16_t size;
  uint8_t message_type;
  uint8_t stream_type;
  uint8_t codec_or_sample_format;
  uint8_t num_channels;
  uint16_t sample_rate;
  int64_t timestamp_or_frames;
};

}  // namespace capture_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_PACKET_HEADER_H_
