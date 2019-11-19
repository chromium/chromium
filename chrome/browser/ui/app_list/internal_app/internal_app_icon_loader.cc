// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/internal_app/internal_app_icon_loader.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"

InternalAppIconLoader::InternalAppIconLoader(Profile* profile,
                                             int resource_size_in_dip,
                                             AppIconLoaderDelegate* delegate)
    : AppIconLoader(profile, resource_size_in_dip, delegate) {}

InternalAppIconLoader::~InternalAppIconLoader() = default;

bool InternalAppIconLoader::CanLoadImageForApp(const std::string& app_id) {
  if (icon_map_.find(app_id) != icon_map_.end())
    return true;
  return app_list::IsInternalApp(app_id);
}

void InternalAppIconLoader::FetchImage(const std::string& app_id) {
  if (icon_map_.find(app_id) != icon_map_.end())
    return;

  gfx::ImageSkia image_skia(app_list::GetIconForResourceId(
      app_list::GetIconResourceIdByAppId(app_id), icon_size_in_dip()));
  image_skia.EnsureRepsForSupportedScales();
  icon_map_[app_id] = image_skia;
  UpdateImage(app_id);
}

void InternalAppIconLoader::ClearImage(const std::string& app_id) {
  icon_map_.erase(app_id);
}

void InternalAppIconLoader::UpdateImage(const std::string& app_id) {
  AppIDToIconMap::const_iterator it = icon_map_.find(app_id);
  if (it == icon_map_.end())
    return;

  delegate()->OnAppImageUpdated(app_id, it->second);
}
