// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_PROTO_HELPERS_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_PROTO_HELPERS_H_

#include "base/memory/scoped_refptr.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace net {
class IOBufferWithSize;
}  // namespace net

namespace chromecast {
class SmallMessageSocket;

namespace media {
namespace mixer_service {
class Generic;

scoped_refptr<net::IOBufferWithSize> SendProto(
    const google::protobuf::MessageLite& message,
    SmallMessageSocket* socket);
bool ReceiveProto(const char* data, int size, Generic* message);

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_PROTO_HELPERS_H_
