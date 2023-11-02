// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_PACKET_HEADER_H_
#define CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_PACKET_HEADER_H_

#include <cstdint>

namespace chromecast {
namespace media {
namespace capture_service {

// Memory block of a PCM audio packet header. Changes to it need to ensure the
// size is a multiple of 4 bytes. It reflects real packet header structure,
// however, the |size| bits are in big-endian order, and thus is only for
// padding purpose in this struct, when all bytes after it represent a message
// header.
struct __attribute__((__packed__)) PcmPacketHeader {
  uint16_t size;
  uint8_t message_type;
  uint8_t stream_type;
  int64_t timestamp_us;
};

// Memory block of a handshake packet. Audio packet may be sent after handshake
// packet, and thus handshake packet must also have a multiple of 4 bytes, since
// audio data must be aligned.
struct __attribute__((__packed__)) HandshakePacket {
  uint16_t size;
  uint8_t message_type;
  uint8_t stream_type;
  uint8_t audio_codec;
  uint8_t sample_format;
  uint8_t num_channels;
  uint8_t padding_uint8;
  uint16_t num_frames;
  uint16_t padding_uint16;
  uint32_t sample_rate;
};

}  // namespace capture_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_PACKET_HEADER_H_
