// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/themes_and_customization_handler.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_metrics.h"

ThemesAndCustomizationHandler::ThemesAndCustomizationHandler(
    mojo::PendingReceiver<
        feature_showcase::mojom::ThemesAndCustomizationPageHandler> receiver,
    ThemeService* theme_service)
    : receiver_(this, std::move(receiver)), theme_service_(theme_service) {
  if (theme_service_) {
    theme_service_observation_.Observe(theme_service_);
  }
}

ThemesAndCustomizationHandler::~ThemesAndCustomizationHandler() {
  if (revert_theme_on_destruction_) {
    RevertThemeInternal();
  }
}

void ThemesAndCustomizationHandler::SnapshotTheme() {
  if (theme_service_) {
    original_color_scheme_ = theme_service_->GetBrowserColorScheme();
    theme_reinstaller_ = theme_service_->BuildReinstallerForCurrentTheme();
  }
}

void ThemesAndCustomizationHandler::AcceptTheme() {
  RecordStepUserAction(FeatureShowcaseStep::kThemesAndCustomization,
                       FeatureShowcaseStepUserAction::kAccepted);
  revert_theme_on_destruction_ = false;
}

void ThemesAndCustomizationHandler::RevertTheme() {
  RecordStepUserAction(FeatureShowcaseStep::kThemesAndCustomization,
                       FeatureShowcaseStepUserAction::kDeclined);
  if (!theme_changed_recorded_) {
    base::UmaHistogramBoolean(
        "ProfilePicker.FREFlow.FeatureShowcase."
        "ThemesAndCustomization.ThemeChanged",
        false);
    theme_changed_recorded_ = true;
  }
  RevertThemeInternal();
}

void ThemesAndCustomizationHandler::OnThemeChanged() {
  if (theme_changed_recorded_) {
    return;
  }

  theme_changed_recorded_ = true;
  base::UmaHistogramBoolean(
      "ProfilePicker.FREFlow.FeatureShowcase.ThemesAndCustomization."
      "ThemeChanged",
      true);
}

void ThemesAndCustomizationHandler::RevertThemeInternal() {
  CHECK(revert_theme_on_destruction_);
  revert_theme_on_destruction_ = false;

  if (!theme_service_) {
    return;
  }
  theme_service_->SetBrowserColorScheme(original_color_scheme_);
  theme_reinstaller_->Reinstall();
}
