// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/native_theme_cache.h"

#include "chromeos/lacros/lacros_service.h"
#include "ui/native_theme/native_theme_aura.h"

namespace chromeos {

NativeThemeCache::NativeThemeCache(const crosapi::mojom::NativeThemeInfo& info)
    : info_(info.Clone()) {
  SetNativeThemeInfo();
}

NativeThemeCache::~NativeThemeCache() = default;

void NativeThemeCache::Start() {
  auto* lacros_service = chromeos::LacrosService::Get();
  CHECK(lacros_service->IsAvailable<crosapi::mojom::NativeThemeService>());
  lacros_service->GetRemote<crosapi::mojom::NativeThemeService>()
      ->AddNativeThemeInfoObserver(receiver_.BindNewPipeAndPassRemote());
}

void NativeThemeCache::OnNativeThemeInfoChanged(
    crosapi::mojom::NativeThemeInfoPtr info) {
  info_ = std::move(info);

  SetNativeThemeInfo();
}

void NativeThemeCache::SetNativeThemeInfo() {
  bool dark_mode = info_->dark_mode || ui::NativeTheme::IsForcedDarkMode();
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  native_theme->set_use_dark_colors(dark_mode);
  native_theme->NotifyOnNativeThemeUpdated();

  auto* native_theme_web = ui::NativeTheme::GetInstanceForWeb();
  native_theme_web->set_use_dark_colors(dark_mode);
  native_theme_web->set_preferred_color_scheme(
      dark_mode ? ui::NativeTheme::PreferredColorScheme::kDark
                : ui::NativeTheme::PreferredColorScheme::kLight);
  native_theme_web->NotifyOnNativeThemeUpdated();
}

}  // namespace chromeos
