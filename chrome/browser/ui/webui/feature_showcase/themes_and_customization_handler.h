// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_THEMES_AND_CUSTOMIZATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_THEMES_AND_CUSTOMIZATION_HANDLER_H_

#include <optional>
#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/webui/feature_showcase/themes_and_customization.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/mojom/themes.mojom.h"

class ThemesAndCustomizationHandler
    : public feature_showcase::mojom::ThemesAndCustomizationPageHandler,
      public ThemeServiceObserver {
 public:
  ThemesAndCustomizationHandler(
      mojo::PendingReceiver<
          feature_showcase::mojom::ThemesAndCustomizationPageHandler> receiver,
      ThemeService* theme_service);
  ThemesAndCustomizationHandler(const ThemesAndCustomizationHandler&) = delete;
  ThemesAndCustomizationHandler& operator=(
      const ThemesAndCustomizationHandler&) = delete;
  ~ThemesAndCustomizationHandler() override;

  // feature_showcase::mojom::ThemesAndCustomizationPageHandler:
  void SnapshotTheme() override;
  void AcceptTheme() override;
  void RevertTheme() override;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

 private:
  void RevertThemeInternal();

  mojo::Receiver<feature_showcase::mojom::ThemesAndCustomizationPageHandler>
      receiver_;
  raw_ptr<ThemeService> theme_service_;
  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_service_observation_{this};
  bool revert_theme_on_destruction_ = true;
  bool theme_changed_recorded_ = false;
  ThemeService::BrowserColorScheme original_color_scheme_ =
      ThemeService::BrowserColorScheme::kSystem;
  std::unique_ptr<ThemeService::ThemeReinstaller> theme_reinstaller_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_THEMES_AND_CUSTOMIZATION_HANDLER_H_
