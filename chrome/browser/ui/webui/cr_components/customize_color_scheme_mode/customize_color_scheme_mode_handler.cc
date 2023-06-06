// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/customize_color_scheme_mode/customize_color_scheme_mode_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "ui/webui/resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.mojom.h"

CustomizeColorSchemeModeHandler::CustomizeColorSchemeModeHandler(
    mojo::PendingRemote<
        customize_color_scheme_mode::mojom::CustomizeColorSchemeModeClient>
        pending_client,
    mojo::PendingReceiver<
        customize_color_scheme_mode::mojom::CustomizeColorSchemeModeHandler>
        pending_handler,
    Profile* profile)
    : remote_client_(std::move(pending_client)),
      receiver_(this, std::move(pending_handler)),
      theme_service_(ThemeServiceFactory::GetForProfile(profile)) {
  CHECK(theme_service_);
  theme_service_->AddObserver(this);
}

CustomizeColorSchemeModeHandler::~CustomizeColorSchemeModeHandler() {
  theme_service_->RemoveObserver(this);
}

void CustomizeColorSchemeModeHandler::SetColorSchemeMode(
    customize_color_scheme_mode::mojom::ColorSchemeMode colorMode) {
  theme_service_->SetBrowserColorScheme(
      static_cast<ThemeService::BrowserColorScheme>(colorMode));
}

void CustomizeColorSchemeModeHandler::InitializeColorSchemeMode() {
  CustomizeColorSchemeModeHandler::UpdateColorSchemeMode();
}

void CustomizeColorSchemeModeHandler::OnThemeChanged() {
  CustomizeColorSchemeModeHandler::UpdateColorSchemeMode();
}

void CustomizeColorSchemeModeHandler::UpdateColorSchemeMode() {
  remote_client_->SetColorSchemeMode(
      static_cast<customize_color_scheme_mode::mojom::ColorSchemeMode>(
          theme_service_->GetBrowserColorScheme()));
}
