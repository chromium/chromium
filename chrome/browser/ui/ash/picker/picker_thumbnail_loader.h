// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PICKER_PICKER_THUMBNAIL_LOADER_H_
#define CHROME_BROWSER_UI_ASH_PICKER_PICKER_THUMBNAIL_LOADER_H_

#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/ash/thumbnail_loader.h"

class SkBitmap;

namespace base {
class FilePath;
}

namespace gfx {
class Size;
}

class PickerThumbnailLoader {
 public:
  using LoadCallback =
      base::OnceCallback<void(const SkBitmap* bitmap, base::File::Error error)>;

  explicit PickerThumbnailLoader(Profile* profile);
  ~PickerThumbnailLoader();
  PickerThumbnailLoader(const PickerThumbnailLoader&) = delete;
  PickerThumbnailLoader& operator=(const PickerThumbnailLoader&) = delete;

  void Load(const base::FilePath& path,
            const gfx::Size& size,
            LoadCallback callback);

 private:
  ash::ThumbnailLoader thumbnail_loader_;
};

#endif  // CHROME_BROWSER_UI_ASH_PICKER_PICKER_THUMBNAIL_LOADER_H_
