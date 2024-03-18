// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/native_theme_cache.h"

#include <optional>

#include "base/time/time.h"
#include "chromeos/crosapi/mojom/native_theme.mojom-forward.h"
#include "chromeos/lacros/lacros_service.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_aura.h"

namespace chromeos {
namespace {

ui::ColorProviderKey::SchemeVariant ConvertSchemeVariant(
    color::mojom::SchemeVariant scheme) {
  switch (scheme) {
    case color::mojom::SchemeVariant::kTonalSpot:
      return ui::ColorProviderKey::SchemeVariant::kTonalSpot;
    case color::mojom::SchemeVariant::kNeutral:
      return ui::ColorProviderKey::SchemeVariant::kNeutral;
    case color::mojom::SchemeVariant::kVibrant:
      return ui::ColorProviderKey::SchemeVariant::kVibrant;
    case color::mojom::SchemeVariant::kExpressive:
      return ui::ColorProviderKey::SchemeVariant::kExpressive;
  }
  // not reached
}

void SetSeedOnTheme(ui::NativeTheme* theme,
                    const crosapi::mojom::NativeThemeInfoPtr& info) {
  if (!info->seed_color.has_value()) {
    return;
  }

  theme->set_user_color(*info->seed_color);
}

void SetVariantOnTheme(ui::NativeTheme* theme,
                       const crosapi::mojom::NativeThemeInfoPtr& info) {
  if (!info->scheme_variant.has_value()) {
    return;
  }

  // TODO(b/286891722): Convert to an EnumTrait after circular dependency is
  // resolved.
  theme->set_scheme_variant(ConvertSchemeVariant(*info->scheme_variant));
}

void SetCaretBlinkIntervalOnTheme(
    ui::NativeTheme* theme,
    const crosapi::mojom::NativeThemeInfoPtr& info) {
  if (!info->caret_blink_interval.has_value()) {
    // A missing value indicates we should use the existing default within
    // NativeTheme, so no further work is required.
    return;
  }
  theme->set_caret_blink_interval(info->caret_blink_interval.value());
}

}  // namespace

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
  SetSeedOnTheme(native_theme, info_);
  SetVariantOnTheme(native_theme, info_);
  SetCaretBlinkIntervalOnTheme(native_theme, info_);
  native_theme->NotifyOnNativeThemeUpdated();

  auto* native_theme_web = ui::NativeTheme::GetInstanceForWeb();
  native_theme_web->set_use_dark_colors(dark_mode);
  native_theme_web->set_preferred_color_scheme(
      dark_mode ? ui::NativeTheme::PreferredColorScheme::kDark
                : ui::NativeTheme::PreferredColorScheme::kLight);
  SetSeedOnTheme(native_theme_web, info_);
  SetVariantOnTheme(native_theme_web, info_);
  SetCaretBlinkIntervalOnTheme(native_theme_web, info_);
  native_theme_web->NotifyOnNativeThemeUpdated();
}

}  // namespace chromeos
