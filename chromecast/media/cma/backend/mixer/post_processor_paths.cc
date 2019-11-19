// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/post_processor_paths.h"

namespace chromecast {
namespace media {

namespace {

const char kPreferredLibraryPath[] = "/system/chrome/lib/processors";
const char kPreferredOemLibraryPath[] = "/oem_cast_shlib/processors";

}  // namespace

// static
base::FilePath GetPostProcessorDirectory() {
  return base::FilePath(kPreferredLibraryPath);
}

// static
base::FilePath GetOemPostProcessorDirectory() {
  return base::FilePath(kPreferredOemLibraryPath);
}

}  // namespace media
}  // namespace chromecast
