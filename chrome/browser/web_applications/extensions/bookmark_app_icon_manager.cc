// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_icon_manager.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace extensions {

namespace {

void OnExtensionIconLoaded(BookmarkAppIconManager::ReadIconCallback callback,
                           const gfx::Image& image) {
  std::move(callback).Run(image.IsEmpty() ? SkBitmap() : *image.ToSkBitmap());
}

bool ReadExtensionIcon(Profile* profile,
                       const web_app::AppId& app_id,
                       int icon_size_in_px,
                       ExtensionIconSet::MatchType match_type,
                       BookmarkAppIconManager::ReadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const Extension* extension =
      ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(app_id);
  if (!extension)
    return false;
  DCHECK(extension->from_bookmark());

  ImageLoader* loader = ImageLoader::Get(profile);
  loader->LoadImageAsync(
      extension,
      IconsInfo::GetIconResource(extension, icon_size_in_px, match_type),
      gfx::Size(icon_size_in_px, icon_size_in_px),
      base::BindOnce(&OnExtensionIconLoaded, std::move(callback)));
  return true;
}

}  // anonymous namespace

BookmarkAppIconManager::BookmarkAppIconManager(Profile* profile)
    : profile_(profile) {}

BookmarkAppIconManager::~BookmarkAppIconManager() = default;

bool BookmarkAppIconManager::ReadIcon(const web_app::AppId& app_id,
                                      int icon_size_in_px,
                                      ReadIconCallback callback) {
  return ReadExtensionIcon(profile_, app_id, icon_size_in_px,
                           ExtensionIconSet::MATCH_EXACTLY,
                           std::move(callback));
}

bool BookmarkAppIconManager::ReadSmallestIcon(const web_app::AppId& app_id,
                                              int icon_size_in_px,
                                              ReadIconCallback callback) {
  return ReadExtensionIcon(profile_, app_id, icon_size_in_px,
                           ExtensionIconSet::MATCH_BIGGER, std::move(callback));
}

}  // namespace extensions
