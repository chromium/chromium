// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/cart/chrome_cart.mojom.h"
#include "chrome/browser/new_tab_page/modules/drive/drive.mojom.h"
#include "chrome/browser/new_tab_page/modules/photos/photos.mojom.h"
#include "chrome/browser/new_tab_page/modules/task_module/task_module.mojom.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"
#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/ui/webui/new_tab_page/foo/foo.mojom.h"  // nogncheck crbug.com/1125897
#endif
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_observer.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/browser/ui/webui/realbox/realbox.mojom-forward.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/customize_themes/customize_themes.mojom.h"
#include "ui/webui/resources/cr_components/most_visited/most_visited.mojom.h"

namespace base {
class RefCountedMemory;
}

namespace content {
class NavigationHandle;
class WebContents;
class WebUI;
}  // namespace content

class ChromeCustomizeThemesHandler;
#if !defined(OFFICIAL_BUILD)
class FooHandler;
#endif
class GURL;
class MostVisitedHandler;
class NewTabPageHandler;
class PrefRegistrySimple;
class PrefService;
class Profile;
class BrowserCommandHandler;
class RealboxHandler;
class TaskModuleHandler;
class CartHandler;
class DriveHandler;
class PhotosHandler;

class NewTabPageUI
    : public ui::MojoWebUIController,
      public new_tab_page::mojom::PageHandlerFactory,
      public customize_themes::mojom::CustomizeThemesHandlerFactory,
      public most_visited::mojom::MostVisitedPageHandlerFactory,
      public browser_command::mojom::CommandHandlerFactory,
      public ui::NativeThemeObserver,
      public ThemeServiceObserver,
      public NtpCustomBackgroundServiceObserver,
      content::WebContentsObserver {
 public:
  explicit NewTabPageUI(content::WebUI* web_ui);

  NewTabPageUI(const NewTabPageUI&) = delete;
  NewTabPageUI& operator=(const NewTabPageUI&) = delete;

  ~NewTabPageUI() override;

  static bool IsNewTabPageOrigin(const GURL& url);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static void ResetProfilePrefs(PrefService* prefs);
  static bool IsDriveModuleEnabled(Profile* profile);

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<new_tab_page::mojom::PageHandlerFactory>
          pending_receiver);

  // Instantiates the implementor of the realbox::mojom::PageHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<realbox::mojom::PageHandler> pending_page_handler);

  // Instantiates the implementor of the
  // browser_command::mojom::CommandHandlerFactory mojo interface passing
  // the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<browser_command::mojom::CommandHandlerFactory>
          pending_receiver);

  // Instantiates the implementor of the
  // customize_themes::mojom::CustomizeThemesHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<
                     customize_themes::mojom::CustomizeThemesHandlerFactory>
                         pending_receiver);

  // Instantiates the implementor of the
  // most_visited::mojom::MostVisitedPageHandlerFactory mojo interface passing
  // the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandlerFactory>
          pending_receiver);

  // Instantiates the implementor of the
  // recipe_tasks::mojom::RecipeTasksHandler mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<task_module::mojom::TaskModuleHandler>
          pending_receiver);

  // Instantiates the implementor of drive::mojom::DriveHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<drive::mojom::DriveHandler> pending_receiver);

  // Instantiates the implementor of photos::mojom::PhotosHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<photos::mojom::PhotosHandler> pending_receiver);

#if !defined(OFFICIAL_BUILD)
  // Instantiates the implementor of the foo::mojom::FooHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<foo::mojom::FooHandler> pending_receiver);
#endif

  // Instantiates the implementor of the chrome_cart::mojom::CartHandler
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<chrome_cart::mojom::CartHandler> pending_receiver);

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

 private:
  // new_tab_page::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
      mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
          pending_page_handler) override;

  // customize_themes::mojom::CustomizeThemesHandlerFactory:
  void CreateCustomizeThemesHandler(
      mojo::PendingRemote<customize_themes::mojom::CustomizeThemesClient>
          pending_client,
      mojo::PendingReceiver<customize_themes::mojom::CustomizeThemesHandler>
          pending_handler) override;

  // browser_command::mojom::CommandHandlerFactory
  void CreateBrowserCommandHandler(
      mojo::PendingReceiver<browser_command::mojom::CommandHandler>
          pending_handler) override;

  // most_visited::mojom::MostVisitedPageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<most_visited::mojom::MostVisitedPage> pending_page,
      mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandler>
          pending_page_handler) override;

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

  // NtpCustomBackgroundServiceObserver:
  void OnCustomBackgroundImageUpdated() override;
  void OnNtpCustomBackgroundServiceShuttingDown() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  bool IsCustomLinksEnabled() const;
  bool IsShortcutsVisible() const;

  // Callback for when the value of the pref for showing custom links vs. most
  // visited sites in the NTP tiles changes.
  void OnCustomLinksEnabledPrefChanged();
  // Callback for when the value of the pref for showing the NTP tiles changes.
  void OnTilesVisibilityPrefChanged();
  // Called when the NTP (re)loads. Sets mutable load time data.
  void OnLoad();

  std::unique_ptr<NewTabPageHandler> page_handler_;
  mojo::Receiver<new_tab_page::mojom::PageHandlerFactory>
      page_factory_receiver_;
  std::unique_ptr<ChromeCustomizeThemesHandler> customize_themes_handler_;
  mojo::Receiver<customize_themes::mojom::CustomizeThemesHandlerFactory>
      customize_themes_factory_receiver_;
  std::unique_ptr<MostVisitedHandler> most_visited_page_handler_;
  mojo::Receiver<most_visited::mojom::MostVisitedPageHandlerFactory>
      most_visited_page_factory_receiver_;
  std::unique_ptr<BrowserCommandHandler> promo_browser_command_handler_;
  mojo::Receiver<browser_command::mojom::CommandHandlerFactory>
      browser_command_factory_receiver_;
  std::unique_ptr<RealboxHandler> realbox_handler_;
#if !defined(OFFICIAL_BUILD)
  std::unique_ptr<FooHandler> foo_handler_;
#endif
  std::unique_ptr<CartHandler> cart_handler_;
  raw_ptr<Profile> profile_;
  raw_ptr<ThemeService> theme_service_;
  raw_ptr<NtpCustomBackgroundService> ntp_custom_background_service_;
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};
  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_service_observation_{this};
  base::ScopedObservation<NtpCustomBackgroundService,
                          NtpCustomBackgroundServiceObserver>
      ntp_custom_background_service_observation_{this};
  raw_ptr<content::WebContents> web_contents_;
  // Time the NTP started loading. Used for logging the WebUI NTP's load
  // performance.
  base::Time navigation_start_time_;

  // Mojo implementations for modules:
  std::unique_ptr<TaskModuleHandler> task_module_handler_;
  std::unique_ptr<DriveHandler> drive_handler_;
  std::unique_ptr<PhotosHandler> photos_handler_;

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<NewTabPageUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_
