// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_UI_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/cart/chrome_cart.mojom.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome.mojom.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/wallpaper_search/wallpaper_search.mojom.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"
#include "ui/webui/resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.mojom.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"
#include "ui/webui/resources/cr_components/theme_color_picker/theme_color_picker.mojom.h"

namespace content {
class WebContents;
}  // namespace content

class CustomizeChromePageHandler;
class CartHandler;
class Profile;
class HelpBubbleHandler;
class CustomizeColorSchemeModeHandler;
class ThemeColorPickerHandler;
class WallpaperSearchBackgroundManager;
class WallpaperSearchHandler;

namespace ui {
class ColorChangeHandler;
}

// WebUI controller for chrome://customize-chrome-side-panel.top-chrome
class CustomizeChromeUI
    : public ui::MojoBubbleWebUIController,
      public help_bubble::mojom::HelpBubbleHandlerFactory,
      public customize_color_scheme_mode::mojom::
          CustomizeColorSchemeModeHandlerFactory,
      public theme_color_picker::mojom::ThemeColorPickerHandlerFactory,
      public side_panel::customize_chrome::mojom::WallpaperSearchHandlerFactory,
      public side_panel::mojom::CustomizeChromePageHandlerFactory {
 public:
  explicit CustomizeChromeUI(content::WebUI* web_ui);
  CustomizeChromeUI(const CustomizeChromeUI&) = delete;
  CustomizeChromeUI& operator=(const CustomizeChromeUI&) = delete;

  ~CustomizeChromeUI() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kChangeChromeThemeButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kChangeChromeThemeClassicElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kChromeThemeBackElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kChromeThemeCollectionElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kChromeThemeElementId);

  void ScrollToSection(CustomizeChromeSection section);

  // Gets a weak pointer to this object.
  base::WeakPtr<CustomizeChromeUI> GetWeakPtr();

  // Instantiates the implementor of the
  // mojom::CustomizeChromePageHandler mojo interface passing the pending
  // receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          side_panel::mojom::CustomizeChromePageHandlerFactory> receiver);

  // Instantiates the implementor of the chrome_cart::mojom::CartHandler
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<chrome_cart::mojom::CartHandler> pending_receiver);

  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          pending_receiver);

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);

  void BindInterface(
      mojo::PendingReceiver<customize_color_scheme_mode::mojom::
                                CustomizeColorSchemeModeHandlerFactory>
          pending_receiver);

  void BindInterface(mojo::PendingReceiver<
                     theme_color_picker::mojom::ThemeColorPickerHandlerFactory>
                         pending_receiver);

  void BindInterface(
      mojo::PendingReceiver<
          side_panel::customize_chrome::mojom::WallpaperSearchHandlerFactory>
          pending_receiver);

 private:
  // side_panel::mojom::CustomizeChromePageHandlerFactory
  void CreatePageHandler(
      mojo::PendingRemote<side_panel::mojom::CustomizeChromePage> pending_page,
      mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>
          pending_page_handler) override;

  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

  // customize_color_scheme_mode::mojom::CustomizeColorSchemeModeHandlerFactory:
  void CreateCustomizeColorSchemeModeHandler(
      mojo::PendingRemote<
          customize_color_scheme_mode::mojom::CustomizeColorSchemeModeClient>
          client,
      mojo::PendingReceiver<
          customize_color_scheme_mode::mojom::CustomizeColorSchemeModeHandler>
          handler) override;

  // theme_color_picker::mojom::ThemeColorPickerHandlerFactory:
  void CreateThemeColorPickerHandler(
      mojo::PendingReceiver<theme_color_picker::mojom::ThemeColorPickerHandler>
          handler,
      mojo::PendingRemote<theme_color_picker::mojom::ThemeColorPickerClient>
          client) override;

  // side_panel::customize_chrome::mojom::WallpaperSearchHandlerFactory:
  void CreateWallpaperSearchHandler(
      mojo::PendingRemote<
          side_panel::customize_chrome::mojom::WallpaperSearchClient> client,
      mojo::PendingReceiver<
          side_panel::customize_chrome::mojom::WallpaperSearchHandler> handler)
      override;

  // image_decoder_ needs to be initialized before
  // wallpaper_search_handler_ so that the image decoder will be
  // deconstructed after the handler. Otherwise, we will get a dangling pointer
  // error from the raw_ptr in the handler not pointing to anything after
  // image_decoder_ object is deleted.
  std::unique_ptr<ImageDecoderImpl> image_decoder_;
  std::unique_ptr<CustomizeChromePageHandler> customize_chrome_page_handler_;
  std::unique_ptr<CartHandler> cart_handler_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  const std::vector<std::pair<const std::string, int>> module_id_names_;
  mojo::Receiver<side_panel::mojom::CustomizeChromePageHandlerFactory>
      page_factory_receiver_;
  // Caches a request to scroll to a section in case the request happens before
  // the front-end is ready to receive the request.
  absl::optional<CustomizeChromeSection> section_;

  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_handler_factory_receiver_{this};
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  std::unique_ptr<CustomizeColorSchemeModeHandler>
      customize_color_scheme_mode_handler_;
  mojo::Receiver<customize_color_scheme_mode::mojom::
                     CustomizeColorSchemeModeHandlerFactory>
      customize_color_scheme_mode_handler_factory_receiver_{this};
  std::unique_ptr<ThemeColorPickerHandler> theme_color_picker_handler_;
  mojo::Receiver<theme_color_picker::mojom::ThemeColorPickerHandlerFactory>
      theme_color_picker_handler_factory_receiver_{this};
  std::unique_ptr<WallpaperSearchBackgroundManager>
      wallpaper_search_background_manager_;
  std::unique_ptr<WallpaperSearchHandler> wallpaper_search_handler_;
  mojo::Receiver<
      side_panel::customize_chrome::mojom::WallpaperSearchHandlerFactory>
      wallpaper_search_handler_factory_receiver_{this};
  const int64_t id_;

  base::WeakPtrFactory<CustomizeChromeUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_UI_H_
