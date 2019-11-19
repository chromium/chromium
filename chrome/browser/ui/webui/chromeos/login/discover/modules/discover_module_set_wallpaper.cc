// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/discover/modules/discover_module_set_wallpaper.h"

#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {

namespace {

class DiscoverModuleSetWallpaperHandler : public DiscoverHandler {
 public:
  explicit DiscoverModuleSetWallpaperHandler(
      JSCallsContainer* js_calls_container);
  ~DiscoverModuleSetWallpaperHandler() override = default;

  // BaseWebUIHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;
  void RegisterMessages() override;

 private:
  // Message handlers.
  void HandleLaunchWallpaperPicker();

  DISALLOW_COPY_AND_ASSIGN(DiscoverModuleSetWallpaperHandler);
};

DiscoverModuleSetWallpaperHandler::DiscoverModuleSetWallpaperHandler(
    JSCallsContainer* js_calls_container)
    : DiscoverHandler(js_calls_container) {}

void DiscoverModuleSetWallpaperHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("discoverSetWallpaper", IDS_DISCOVER_SET_WALLPAPER);
}

void DiscoverModuleSetWallpaperHandler::Initialize() {}

void DiscoverModuleSetWallpaperHandler::RegisterMessages() {
  AddCallback("discover.setWallpaper.launchWallpaperPicker",
              &DiscoverModuleSetWallpaperHandler::HandleLaunchWallpaperPicker);
}

void DiscoverModuleSetWallpaperHandler::HandleLaunchWallpaperPicker() {
  WallpaperControllerClient::Get()->OpenWallpaperPickerIfAllowed();
}

}  // anonymous namespace

/* ***************************************************************** */
/* Discover SetWallpaper module implementation below.                */

constexpr char DiscoverModuleSetWallpaper::kModuleName[];

DiscoverModuleSetWallpaper::DiscoverModuleSetWallpaper() = default;

DiscoverModuleSetWallpaper::~DiscoverModuleSetWallpaper() = default;

bool DiscoverModuleSetWallpaper::IsCompleted() const {
  // TODO (alemate, https://crbug.com/864677/):
  // 1) We need to support ShouldShowWallpaperSetting .
  // 2) IsActiveUserWallpaperControlledByPolicy .
  return false;
}

std::unique_ptr<DiscoverHandler> DiscoverModuleSetWallpaper::CreateWebUIHandler(
    JSCallsContainer* js_calls_container) {
  return std::make_unique<DiscoverModuleSetWallpaperHandler>(
      js_calls_container);
}

}  // namespace chromeos
