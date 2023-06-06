// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_CUSTOMIZE_COLOR_SCHEME_MODE_CUSTOMIZE_COLOR_SCHEME_MODE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_CUSTOMIZE_COLOR_SCHEME_MODE_CUSTOMIZE_COLOR_SCHEME_MODE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.mojom.h"

class Profile;
class ThemeService;

class CustomizeColorSchemeModeHandler
    : public customize_color_scheme_mode::mojom::
          CustomizeColorSchemeModeHandler,
      public ThemeServiceObserver {
 public:
  explicit CustomizeColorSchemeModeHandler(
      mojo::PendingRemote<
          customize_color_scheme_mode::mojom::CustomizeColorSchemeModeClient>
          pending_client,
      mojo::PendingReceiver<
          customize_color_scheme_mode::mojom::CustomizeColorSchemeModeHandler>
          pending_handler,
      Profile* profile);
  ~CustomizeColorSchemeModeHandler() override;

  // customize_color_scheme_mode::mojom::CustomizeColorSchemeModeHandler:
  void SetColorSchemeMode(
      customize_color_scheme_mode::mojom::ColorSchemeMode colorMode) override;
  void InitializeColorSchemeMode() override;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

 private:
  void UpdateColorSchemeMode();

  mojo::Remote<
      customize_color_scheme_mode::mojom::CustomizeColorSchemeModeClient>
      remote_client_;
  mojo::Receiver<
      customize_color_scheme_mode::mojom::CustomizeColorSchemeModeHandler>
      receiver_;

  const raw_ptr<ThemeService> theme_service_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_CUSTOMIZE_COLOR_SCHEME_MODE_CUSTOMIZE_COLOR_SCHEME_MODE_HANDLER_H_
