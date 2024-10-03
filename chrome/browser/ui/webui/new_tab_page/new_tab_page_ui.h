// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_

#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/file_suggestion.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption.mojom.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"
#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/ui/webui/new_tab_page/foo/foo.mojom.h"  // nogncheck crbug.com/1125897
#endif
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/search/background/ntp_custom_background_service_observer.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/resources/cr_components/most_visited/most_visited.mojom.h"
#include "ui/webui/resources/cr_components/searchbox/searchbox.mojom-forward.h"
#include "ui/webui/resources/js/metrics_reporter/metrics_reporter.mojom.h"

namespace base {
class RefCountedMemory;
}  // namespace base

namespace content {
class NavigationHandle;
class WebUI;
}  // namespace content

namespace page_image_service {
class ImageServiceHandler;
}  // namespace page_image_service

namespace ui {
class ColorChangeHandler;
}  // namespace ui

class BrowserCommandHandler;
class FileSuggestionHandler;
#if !defined(OFFICIAL_BUILD)
class FooHandler;
#endif
class GoogleCalendarPageHandler;
class OutlookCalendarPageHandler;
class GURL;
class MostRelevantTabResumptionPageHandler;
class MostVisitedHandler;
class NewTabPageHandler;
class NtpCustomBackgroundService;
class PrefRegistrySimple;
class PrefService;
class Profile;
class RealboxHandler;
class NewTabPageUI;

class NewTabPageUIConfig : public content::DefaultWebUIConfig<NewTabPageUI> {
 public:
  NewTabPageUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUINewTabPageHost) {}
  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class NewTabPageUI
    : public ui::MojoWebUIController,
      public new_tab_page::mojom::PageHandlerFactory,
      public most_visited::mojom::MostVisitedPageHandlerFactory,
      public browser_command::mojom::CommandHandlerFactory,
      public help_bubble::mojom::HelpBubbleHandlerFactory,
      public NtpCustomBackgroundServiceObserver,
      content::WebContentsObserver {
 public:
  explicit NewTabPageUI(content::WebUI* web_ui);

  NewTabPageUI(const NewTabPageUI&) = delete;
  NewTabPageUI& operator=(const NewTabPageUI&) = delete;

  ~NewTabPageUI() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCustomizeChromeButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kModulesCustomizeIPHAnchorElement);

  static bool IsNewTabPageOrigin(const GURL& url);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static void ResetProfilePrefs(PrefService* prefs);
  static bool IsManagedProfile(Profile* profile);

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<new_tab_page::mojom::PageHandlerFactory>
          pending_receiver);

  // Instantiates the implementor of the mojom::PageHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);

  // Instantiates the implementor of the searchbox::mojom::PageHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<searchbox::mojom::PageHandler>
                         pending_page_handler);

  // Instantiates the implementor of metrics_reporter::mojom::PageMetricsHost
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<metrics_reporter::mojom::PageMetricsHost> receiver);

  // Instantiates the implementor of the
  // browser_command::mojom::CommandHandlerFactory mojo interface passing
  // the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<browser_command::mojom::CommandHandlerFactory>
          pending_receiver);

  // Instantiates the implementor of the
  // most_visited::mojom::MostVisitedPageHandlerFactory mojo interface passing
  // the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandlerFactory>
          pending_receiver);

  // Instantiates the implementor of
  // file_suggestion::mojom::FileSuggestionHandler mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<file_suggestion::mojom::FileSuggestionHandler>
          pending_receiver);

  // Instantiates the implementor of
  // npt::calendar::mojom::GoogleCalendarPageHandler mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<ntp::calendar::mojom::GoogleCalendarPageHandler>
          pending_receiver);

  // Instantiates the implementor of
  // npt::calendar::mojom::OutlookCalendarPageHandler mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<ntp::calendar::mojom::OutlookCalendarPageHandler>
          pending_receiver);

#if !defined(OFFICIAL_BUILD)
  // Instantiates the implementor of the foo::mojom::FooHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<foo::mojom::FooHandler> pending_receiver);
#endif

  void BindInterface(mojo::PendingReceiver<
                     ntp::most_relevant_tab_resumption::mojom::PageHandler>
                         pending_page_handler);

  void BindInterface(
      mojo::PendingReceiver<page_image_service::mojom::PageImageServiceHandler>
          pending_page_handler);

  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          pending_receiver);

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

 private:
  // new_tab_page::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
      mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
          pending_page_handler) override;

  // browser_command::mojom::CommandHandlerFactory
  void CreateBrowserCommandHandler(
      mojo::PendingReceiver<browser_command::mojom::CommandHandler>
          pending_handler) override;

  // most_visited::mojom::MostVisitedPageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<most_visited::mojom::MostVisitedPage> pending_page,
      mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandler>
          pending_page_handler) override;

  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

  // NtpCustomBackgroundServiceObserver:
  void OnCustomBackgroundImageUpdated() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnColorProviderChanged() override;

  bool IsCustomLinksEnabled() const;
  bool IsShortcutsVisible() const;

  // Callback for when the value of the pref for showing custom links vs. most
  // visited sites in the NTP tiles changes.
  void OnCustomLinksEnabledPrefChanged();
  // Callback for when the value of the pref for showing the NTP tiles changes.
  void OnTilesVisibilityPrefChanged();
  // Called when the NTP (re)loads. Sets mutable load time data.
  void OnLoad();

  // Called when the tab will be detached.
  void TabWillDetach(tabs::TabInterface* tab,
                     tabs::TabInterface::DetachReason reason);

  std::unique_ptr<NewTabPageHandler> page_handler_;
  mojo::Receiver<new_tab_page::mojom::PageHandlerFactory>
      page_factory_receiver_;
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  std::unique_ptr<MostVisitedHandler> most_visited_page_handler_;
  mojo::Receiver<most_visited::mojom::MostVisitedPageHandlerFactory>
      most_visited_page_factory_receiver_;
  std::unique_ptr<BrowserCommandHandler> promo_browser_command_handler_;
  mojo::Receiver<browser_command::mojom::CommandHandlerFactory>
      browser_command_factory_receiver_;
  std::unique_ptr<RealboxHandler> realbox_handler_;
  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_handler_factory_receiver_{this};
  MetricsReporter metrics_reporter_;
#if !defined(OFFICIAL_BUILD)
  std::unique_ptr<FooHandler> foo_handler_;
#endif
  std::unique_ptr<MostRelevantTabResumptionPageHandler>
      most_relevant_tab_resumption_handler_;
  std::unique_ptr<page_image_service::ImageServiceHandler>
      image_service_handler_;
  raw_ptr<Profile> profile_;
  raw_ptr<tabs::TabInterface> tab_;
  raw_ptr<ThemeService> theme_service_;
  raw_ptr<NtpCustomBackgroundService> ntp_custom_background_service_;
  base::ScopedObservation<NtpCustomBackgroundService,
                          NtpCustomBackgroundServiceObserver>
      ntp_custom_background_service_observation_{this};
  // Time the NTP started loading. Used for logging the WebUI NTP's load
  // performance.
  base::Time navigation_start_time_;
  const std::vector<std::pair<const std::string, int>> module_id_names_;

  // Mojo implementations for modules:
  std::unique_ptr<GoogleCalendarPageHandler> google_calendar_handler_;
  std::unique_ptr<FileSuggestionHandler> file_handler_;
  std::unique_ptr<OutlookCalendarPageHandler> outlook_calendar_handler_;
  PrefChangeRegistrar pref_change_registrar_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  base::WeakPtrFactory<NewTabPageUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_
