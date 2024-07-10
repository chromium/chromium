// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_THEME_COLOR_PICKER_THEME_COLOR_PICKER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_THEME_COLOR_PICKER_THEME_COLOR_PICKER_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_observer.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/mojom/themes.mojom.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"
#include "ui/webui/resources/cr_components/theme_color_picker/theme_color_picker.mojom.h"

namespace content {
class WebContents;
}  // namespace content

class Profile;

class ThemeColorPickerHandler
    : public theme_color_picker::mojom::ThemeColorPickerHandler,
      public ui::NativeThemeObserver,
      public ThemeServiceObserver,
      public NtpCustomBackgroundServiceObserver {
 public:
  ThemeColorPickerHandler(
      mojo::PendingReceiver<theme_color_picker::mojom::ThemeColorPickerHandler>
          pending_handler,
      mojo::PendingRemote<theme_color_picker::mojom::ThemeColorPickerClient>
          pending_client,
      NtpCustomBackgroundService* ntp_custom_background_service,
      content::WebContents* web_contents);

  ThemeColorPickerHandler(const ThemeColorPickerHandler&) = delete;
  ThemeColorPickerHandler& operator=(const ThemeColorPickerHandler&) = delete;

  ~ThemeColorPickerHandler() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // side_panel::mojom::CustomizeChromePageHandler:
  void SetDefaultColor() override;
  void SetGreyDefaultColor() override;
  void SetSeedColor(SkColor seed_color,
                    ui::mojom::BrowserColorVariant variant) override;
  void SetSeedColorFromHue(float hue) override;
  void GetChromeColors(bool is_dark_mode,
                       GetChromeColorsCallback callback) override;
  void RemoveBackgroundImage() override;
  void UpdateTheme() override;

 private:
  void MaybeIncrementSeedColorChangeCount();

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

  // NtpCustomBackgroundServiceObserver:
  void OnCustomBackgroundImageUpdated() override;

  bool seed_color_changed_ = false;
  raw_ptr<NtpCustomBackgroundService> ntp_custom_background_service_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<ThemeService> theme_service_;
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};
  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_service_observation_{this};
  base::ScopedObservation<NtpCustomBackgroundService,
                          NtpCustomBackgroundServiceObserver>
      ntp_custom_background_service_observation_{this};

  mojo::Remote<theme_color_picker::mojom::ThemeColorPickerClient> client_;
  mojo::Receiver<theme_color_picker::mojom::ThemeColorPickerHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_THEME_COLOR_PICKER_THEME_COLOR_PICKER_HANDLER_H_
