// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PICKER_PICKER_THUMBNAIL_LOADER_H_
#define CHROME_BROWSER_UI_ASH_PICKER_PICKER_THUMBNAIL_LOADER_H_

#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/ash/thumbnail_loader/thumbnail_loader.h"

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
  void DecodeDriveThumbnail(LoadCallback callback,
                            const gfx::Size& size,
                            const std::optional<std::vector<uint8_t>>& bytes);
  void OnDriveThumbnailDecoded(LoadCallback callback, const SkBitmap& image);

  raw_ptr<Profile> profile_;
  ash::ThumbnailLoader thumbnail_loader_;

  base::WeakPtrFactory<PickerThumbnailLoader> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_PICKER_PICKER_THUMBNAIL_LOADER_H_
