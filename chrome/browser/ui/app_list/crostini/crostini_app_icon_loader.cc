// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_app_icon_loader.h"

#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "ui/base/resource/resource_bundle.h"

CrostiniAppIconLoader::CrostiniAppIconLoader(Profile* profile,
                                             int resource_size_in_dip,
                                             AppIconLoaderDelegate* delegate)
    : AppIconLoader(profile, resource_size_in_dip, delegate),
      registry_service_(
          crostini::CrostiniRegistryServiceFactory::GetForProfile(profile)) {
  DCHECK(registry_service_);
  registry_service_->AddObserver(this);
}

CrostiniAppIconLoader::~CrostiniAppIconLoader() {
  registry_service_->RemoveObserver(this);
}

bool CrostiniAppIconLoader::CanLoadImageForApp(const std::string& app_id) {
  if (icon_map_.find(app_id) != icon_map_.end())
    return true;

  return registry_service_->IsCrostiniShelfAppId(app_id);
}

void CrostiniAppIconLoader::FetchImage(const std::string& app_id) {
  if (icon_map_.find(app_id) != icon_map_.end())
    return;

  std::unique_ptr<CrostiniAppIcon> icon = std::make_unique<CrostiniAppIcon>(
      profile(), app_id, icon_size_in_dip(), this);
  icon->image_skia().EnsureRepsForSupportedScales();
  icon_map_[app_id] = std::move(icon);
  UpdateImage(app_id);
}

void CrostiniAppIconLoader::ClearImage(const std::string& app_id) {
  icon_map_.erase(app_id);
}

void CrostiniAppIconLoader::UpdateImage(const std::string& app_id) {
  AppIDToIconMap::const_iterator it = icon_map_.find(app_id);
  if (it == icon_map_.end())
    return;

  delegate()->OnAppImageUpdated(app_id, it->second->image_skia());
}

void CrostiniAppIconLoader::OnIconUpdated(CrostiniAppIcon* icon) {
  UpdateImage(icon->app_id());
}

void CrostiniAppIconLoader::OnAppIconUpdated(const std::string& app_id,
                                             ui::ScaleFactor scale_factor) {
  AppIDToIconMap::const_iterator it = icon_map_.find(app_id);
  if (it == icon_map_.end())
    return;
  it->second->LoadForScaleFactor(scale_factor);
}
