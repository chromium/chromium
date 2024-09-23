// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_MESSAGE_PARSING_UTILS_H_
#define CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_MESSAGE_PARSING_UTILS_H_

#include <cstdint>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "chromecast/media/audio/capture_service/constants.h"
#include "media/base/audio_bus.h"
#include "net/base/io_buffer.h"

namespace chromecast {
namespace media {
namespace capture_service {

// Read message header, check if it matches |stream_info|, retrieve timestamp,
// and return whether success.
// Note |data| here has been parsed firstly by SmallMessageSocket, and thus
// doesn't have <uint16_t size> bits.
bool ReadPcmAudioHeader(const char* data,
                        size_t size,
                        const StreamInfo& stream_info,
                        int64_t* timestamp_us);

// Make a IO buffer for stream message. It will populate the header and copy
// |data| into the message if packet has audio and |data| is not null. The
// returned buffer will have a length of |data_size| + header size. Return
// nullptr if fails. Caller must guarantee the memory of |data| has at least
// |data_size| when has audio.
// Note buffer will be sent with SmallMessageSocket, and thus contains a uint16
// size field in the very first.
scoped_refptr<net::IOBufferWithSize> MakePcmAudioMessage(StreamType stream_type,
                                                         int64_t timestamp_us,
                                                         const char* data,
                                                         size_t data_size);

// Make a IO buffer for handshake message. It will populate the header with
// |stream_info|. Return nullptr if fails.
// Note buffer will be sent with SmallMessageSocket, and thus contains a uint16
// size field in the very first.
scoped_refptr<net::IOBufferWithSize> MakeHandshakeMessage(
    const StreamInfo& stream_info);

// Make a IO buffer for serialized message. It will populate message size and
// type fields, and copy |data| into the message. The returned buffer will have
// a length of |data_size| + sizeof(uint8_t message_type) + sizeof(uint16_t
// size).
// Note serialized data cannot be empty, and the method will fail and return
// null if |data| is null or |data_size| is zero.
//
// **This looks like dead code but is used by internal code.**
scoped_refptr<net::IOBufferWithSize> MakeSerializedMessage(
    MessageType message_type,
    base::span<const uint8_t> data);

// Read the audio data in the message and copy to |audio_bus| based on
// |stream_info|. Return false if fails.
bool ReadDataToAudioBus(const StreamInfo& stream_info,
                        const char* data,
                        size_t size,
                        ::media::AudioBus* audio_bus);

// Read the PCM audio message and copy the audio data to audio bus, as well as
// the timestamp. Return whether success. This will run ReadPcmAudioHeader() and
// ReadDataToAudioBus() in the underlying implementation.
bool ReadPcmAudioMessage(const char* data,
                         size_t size,
                         const StreamInfo& stream_info,
                         int64_t* timestamp_us,
                         ::media::AudioBus* audio_bus);

// Read the handshake message to |stream_info|, and return true on success.
bool ReadHandshakeMessage(const char* data,
                          size_t size,
                          StreamInfo* stream_info);

// Return the expected size of the data of a stream message with |stream_info|.
size_t DataSizeInBytes(const StreamInfo& stream_info);

// Following methods are exposed for unittests:

// Writes into buf:
// - The size of `buf` as 16 bits in big-endian order.
// - The contents of `data`, which must fit into the remaining bytes of `buf`.
//
// Any other bytes in `buf` are left **uninitialized**. The unwritten tail of
// `buf` is returned, which will be empty if all of `buf` was filled.
base::span<uint8_t> FillBuffer(base::span<uint8_t> buf,
                               base::span<const uint8_t> data);

// Populate header of the PCM audio message, including the SmallMessageSocket
// size bits.
// Note this is used by unittest, user should use MakePcmAudioMessage directly.
char* PopulatePcmAudioHeader(char* data,
                             size_t size,
                             StreamType stream_type,
                             int64_t timestamp_us);

// Populate the handshake message, including the SmallMessageSocket size bits.
// Note this is used by unittest, user should use MakeHandshakeMessage directly.
void PopulateHandshakeMessage(char* data,
                              size_t size,
                              const StreamInfo& stream_info);

}  // namespace capture_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_MESSAGE_PARSING_UTILS_H_
