// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/constants.h"

#include "build/build_config.h"

namespace ntp_tiles {

#if BUILDFLAG(IS_ANDROID)
const size_t kMaxNumCustomLinks = 8;
#else
const size_t kMaxNumCustomLinks = 10;
#endif

// If custom links are enabled, an additional tile may be returned making up to
// kMaxNumCustomLinks custom links including the "Add shortcut" button.
const size_t kMaxNumMostVisited = 8;

}  // namespace ntp_tiles
