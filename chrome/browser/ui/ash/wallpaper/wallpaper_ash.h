// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_WALLPAPER_WALLPAPER_ASH_H_
#define CHROME_BROWSER_UI_ASH_WALLPAPER_WALLPAPER_ASH_H_

#include <string>

#include "ash/public/mojom/wallpaper.mojom.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/extension_id.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"

// Ash implementation of the wallpaper extension API.
class WallpaperAsh {
 public:
  // Returns the single instance. Used to avoid circular dependencies with the
  // owning object ChromeBrowserMainExtraPartsAsh.
  static WallpaperAsh* Get();

  WallpaperAsh();
  WallpaperAsh(const WallpaperAsh&) = delete;
  WallpaperAsh& operator=(const WallpaperAsh&) = delete;
  ~WallpaperAsh();

  void SetWallpaper(
      ash::mojom::WallpaperSettingsPtr wallpaper_settings,
      const std::string& extension_id,
      const std::string& extension_name,
      base::OnceCallback<void(ash::mojom::SetWallpaperResultPtr)> callback);

 private:
  void OnWallpaperDecoded(ash::mojom::WallpaperSettingsPtr wallpaper_settings,
                          const SkBitmap& bitmap);
  void SendErrorResult(const std::string& response);
  void SendSuccessResult(const std::vector<uint8_t>& thumbnail_data);

  // The ID of the extension making the current SetWallpaper() call.
  extensions::ExtensionId extension_id_;
  base::OnceCallback<void(ash::mojom::SetWallpaperResultPtr)> pending_callback_;
  data_decoder::DataDecoder data_decoder_;
  base::WeakPtrFactory<WallpaperAsh> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_WALLPAPER_WALLPAPER_ASH_H_
