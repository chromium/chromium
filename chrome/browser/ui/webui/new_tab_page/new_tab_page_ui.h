// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_

#include "base/macros.h"
#include "chrome/browser/cart/chrome_cart.mojom.h"
#include "chrome/browser/promo_browser_command/promo_browser_command.mojom-forward.h"
#include "chrome/browser/search/drive/drive.mojom.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/task_module/task_module.mojom.h"
#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/ui/webui/new_tab_page/foo/foo.mojom.h"  // nogncheck crbug.com/1125897
#endif
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/browser/ui/webui/realbox/realbox.mojom-forward.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/customize_themes/customize_themes.mojom.h"

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
class InstantService;
class NewTabPageHandler;
class Profile;
class PromoBrowserCommandHandler;
class RealboxHandler;
class TaskModuleHandler;
class CartHandler;
class DriveHandler;

class NewTabPageUI
    : public ui::MojoWebUIController,
      public new_tab_page::mojom::PageHandlerFactory,
      public customize_themes::mojom::CustomizeThemesHandlerFactory,
      public InstantServiceObserver,
      content::WebContentsObserver {
 public:
  explicit NewTabPageUI(content::WebUI* web_ui);
  ~NewTabPageUI() override;

  static bool IsNewTabPageOrigin(const GURL& url);

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
  // promo_browser_command::mojom::CommandHandler mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<promo_browser_command::mojom::CommandHandler>
          pending_receiver);

  // Instantiates the implementor of the
  // customize_themes::mojom::CustomizeThemesHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<
                     customize_themes::mojom::CustomizeThemesHandlerFactory>
                         pending_receiver);

  // Instantiates the implementor of the
  // shopping_tasks::mojom::ShoppingTasksHandler mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<task_module::mojom::TaskModuleHandler>
          pending_receiver);

  // Instantiates the implementor of drive::mojom::DriveHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<drive::mojom::DriveHandler> pending_receiver);

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

  // InstantServiceObserver:
  void NtpThemeChanged(const NtpTheme& theme) override;
  void MostVisitedInfoChanged(const InstantMostVisitedInfo& info) override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Updates the load time data with the current theme's background color. That
  // way the background color is available as soon as the page loads and we
  // prevent a potential white flicker.
  void UpdateBackgroundColor(const NtpTheme& theme);

  std::unique_ptr<NewTabPageHandler> page_handler_;
  mojo::Receiver<new_tab_page::mojom::PageHandlerFactory>
      page_factory_receiver_;
  std::unique_ptr<ChromeCustomizeThemesHandler> customize_themes_handler_;
  mojo::Receiver<customize_themes::mojom::CustomizeThemesHandlerFactory>
      customize_themes_factory_receiver_;
  std::unique_ptr<PromoBrowserCommandHandler> promo_browser_command_handler_;
  std::unique_ptr<RealboxHandler> realbox_handler_;
#if !defined(OFFICIAL_BUILD)
  std::unique_ptr<FooHandler> foo_handler_;
#endif
  std::unique_ptr<CartHandler> cart_handler_;
  Profile* profile_;
  InstantService* instant_service_;
  content::WebContents* web_contents_;
  // Time the NTP started loading. Used for logging the WebUI NTP's load
  // performance.
  base::Time navigation_start_time_;

  // Mojo implementations for modules:
  std::unique_ptr<TaskModuleHandler> task_module_handler_;
  std::unique_ptr<DriveHandler> drive_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();

  DISALLOW_COPY_AND_ASSIGN(NewTabPageUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_
