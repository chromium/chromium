// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_CAST_KEY_SYSTEM_H_
#define CHROMECAST_PUBLIC_MEDIA_CAST_KEY_SYSTEM_H_

namespace chromecast {
namespace media {

// Specifies the encryption key system used by a given buffer.
// TODO(yucliu): Remove KEY_SYSTEM_CLEAR_KEY in next 3P Linux Update.
enum CastKeySystem {
  KEY_SYSTEM_NONE = 0,
  KEY_SYSTEM_CLEAR_KEY,
  KEY_SYSTEM_PLAYREADY,
  KEY_SYSTEM_WIDEVINE
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_CAST_KEY_SYSTEM_H_
