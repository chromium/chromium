// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSOR_PATHS_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSOR_PATHS_H_

#include "base/files/file_path.h"

namespace chromecast {
namespace media {

// Returns the preferred directory for 1P Post Processors.
base::FilePath GetPostProcessorDirectory();

// Returns the preferred directory for 3P Post Processors.
base::FilePath GetOemPostProcessorDirectory();

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSOR_PATHS_H_
