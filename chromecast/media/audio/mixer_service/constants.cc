// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/constants.h"

#include "chromecast/media/audio/mixer_service/buildflags.h"

namespace chromecast {
namespace media {
namespace mixer_service {

bool HaveFullMixer() {
  return BUILDFLAG(HAVE_FULL_MIXER);
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
