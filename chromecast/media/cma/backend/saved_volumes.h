// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_SAVED_VOLUMES_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_SAVED_VOLUMES_H_

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {

// Returns the saved volume in dBFS for each stream type loaded from persistent
// storage, or the appropriate default volume if there was no saved volume.
base::flat_map<AudioContentType, double> LoadSavedVolumes(
    const base::FilePath& storage_path);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_SAVED_VOLUMES_H_
