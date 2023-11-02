// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_CAST_KEY_STATUS_H_
#define CHROMECAST_PUBLIC_MEDIA_CAST_KEY_STATUS_H_

namespace chromecast {
namespace media {

// Status of encryption key.  See EME spec for details:
// https://w3c.github.io/encrypted-media/  - not all key status values
// are supported currently.
enum CastKeyStatus { KEY_STATUS_USABLE = 0, KEY_STATUS_EXPIRED };

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_CAST_KEY_STATUS_H_
