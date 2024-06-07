// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/picker/picker_thumbnail_loader.h"

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/gfx/geometry/size.h"

PickerThumbnailLoader::PickerThumbnailLoader(Profile* profile)
    : thumbnail_loader_(profile) {}

PickerThumbnailLoader::~PickerThumbnailLoader() = default;

void PickerThumbnailLoader::Load(const base::FilePath& path,
                                 const gfx::Size& size,
                                 LoadCallback callback) {
  thumbnail_loader_.Load({path, size}, std::move(callback));
}
