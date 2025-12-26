// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_suggestion.mojom.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/microsoft_files.mojom.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/browser/new_tab_page/modules/v2/authentication/microsoft_auth.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo_handler.h"
#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"
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
#include "chrome/browser/ui/webui/customize_buttons/customize_buttons.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/browser/searchbox.mojom-forward.h"
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
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"
#include "ui/webui/resources/cr_components/most_visited/most_visited.mojom.h"

namespace base {
class RefCountedMemory;
}  // namespace base

namespace content {
class NavigationHandle;
class WebUI;
}  // namespace content

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

namespace page_image_service {
class ImageServiceHandler;
}  // namespace page_image_service

class BrowserCommandHandler;
class ComposeboxHandler;
class CustomizeButtonsHandler;
class DriveSuggestionHandler;
#if !defined(OFFICIAL_BUILD)
class FooHandler;
#endif
class GoogleCalendarPageHandler;
class OutlookCalendarPageHandler;
class GURL;
class MicrosoftAuthPageHandler;
class MicrosoftFilesPageHandler;
class MostRelevantTabResumptionPageHandler;
class MostVisitedHandler;
class NewTabPageHandler;
class NtpCustomBackgroundService;
class PrefRegistrySimple;
class PrefService;
class Profile;
class RealboxHandler;
class TabGroupsPageHandler;
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
      public customize_buttons::mojom::CustomizeButtonsHandlerFactory,
      public most_visited::mojom::MostVisitedPageHandlerFactory,
      public composebox::mojom::PageHandlerFactory,
      public browser_command::mojom::CommandHandlerFactory,
      public help_bubble::mojom::HelpBubbleHandlerFactory,
      public ntp_promo::mojom::NtpPromoHandlerFactory,
      public NtpCustomBackgroundServiceObserver,
      public action_chips::mojom::ActionChipsHandlerFactory,
      content::WebContentsObserver {
 public:
  explicit NewTabPageUI(content::WebUI* web_ui);

  NewTabPageUI(const NewTabPageUI&) = delete;
  NewTabPageUI& operator=(const NewTabPageUI&) = delete;

  ~NewTabPageUI() override;

  static bool IsNewTabPageOrigin(const GURL& url);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static void ResetProfilePrefs(PrefService* prefs);
  static void MigrateDeprecatedUseMostVisitedTilesPref(PrefService* prefs);
  static void MigrateDeprecatedShortcutsTypePref(PrefService* prefs);
  static bool IsManagedProfile(Profile* profile);

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<new_tab_page::mojom::PageHandlerFactory>
          pending_receiver);

  // Instantiates the implementor of the searchbox::mojom::PageHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<searchbox::mojom::PageHandler>
                         pending_page_handler);

  // Instantiates the implementor of the
  // browser_command::mojom::CommandHandlerFactory mojo interface passing
  // the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<browser_command::mojom::CommandHandlerFactory>
          pending_receiver);

  // Instantiates the implementor of the
  // customize_buttons::mojom::CustomizeButtonsHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<
                     customize_buttons::mojom::CustomizeButtonsHandlerFactory>
                         pending_receiver);

  // Instantiates the implementor of the
  // most_visited::mojom::MostVisitedPageHandlerFactory mojo interface passing
  // the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandlerFactory>
          pending_receiver);

  // Instantiates the implementor of
  // file_suggestion::mojom::DriveSuggestionHandler mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<file_suggestion::mojom::DriveSuggestionHandler>
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

  // Instantiates the implementor of
  // npt::authentication::mojom::MicrosoftAuthPageHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<
                     ntp::authentication::mojom::MicrosoftAuthPageHandler>
                         pending_receiver);

  // Instantiates the implementor of
  // file_suggestion::mojom::MicrosoftFilesPageHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<file_suggestion::mojom::MicrosoftFilesPageHandler>
          pending_receiver);

  // Instantiates the implementor of the composebox::mojom::PageHandlerFactory
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<composebox::mojom::PageHandlerFactory>
          pending_receiver);

#if !defined(OFFICIAL_BUILD)
  // Instantiates the implementor of the foo::mojom::FooHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<foo::mojom::FooHandler> pending_receiver);
#endif

  void BindInterface(mojo::PendingReceiver<ntp::tab_groups::mojom::PageHandler>
                         pending_page_handler);

  void BindInterface(mojo::PendingReceiver<
                     ntp::most_relevant_tab_resumption::mojom::PageHandler>
                         pending_page_handler);

  void BindInterface(
      mojo::PendingReceiver<page_image_service::mojom::PageImageServiceHandler>
          pending_page_handler);

  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          pending_receiver);

  void BindInterface(
      mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandlerFactory>
          pending_receiver);

  void BindInterface(
      mojo::PendingReceiver<action_chips::mojom::ActionChipsHandlerFactory>
          pending_receiver);

  void ConnectToParentDocument(
      mojo::PendingRemote<new_tab_page::mojom::MicrosoftAuthUntrustedDocument>
          child_page);

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

  // customize_buttons::mojom::CustomizeButtonsHandlerFactory:
  void CreateCustomizeButtonsHandler(
      mojo::PendingRemote<customize_buttons::mojom::CustomizeButtonsDocument>
          pending_page,
      mojo::PendingReceiver<customize_buttons::mojom::CustomizeButtonsHandler>
          pending_page_handler) override;

  // most_visited::mojom::MostVisitedPageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<most_visited::mojom::MostVisitedPage> pending_page,
      mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandler>
          pending_page_handler) override;

  // composebox::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<composebox::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler) override;

  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

  // ntp_promo::mojom::NtpPromoHandlerFactory:
  void CreateNtpPromoHandler(
      mojo::PendingRemote<ntp_promo::mojom::NtpPromoClient> client,
      mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandler> handler)
      override;

  // action_chips::mojom::ActionChipsHandlerFactory:
  void CreateActionChipsHandler(
      mojo::PendingReceiver<action_chips::mojom::ActionChipsHandler> handler,
      mojo::PendingRemote<action_chips::mojom::Page> page) override;

  // NtpCustomBackgroundServiceObserver:
  void OnCustomBackgroundImageUpdated() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnColorProviderChanged() override;

  bool IsShortcutsVisible() const;

  // Updates the NTP tile types based on current preferences.
  void UpdateMostVisitedTileTypes();
  // Callback for when the value of the prefs for determining the type of NTP
  // tiles to show changes.
  void OnTileTypesChanged();
  // Callback for when the value of the pref for showing the NTP tiles changes.
  void OnTilesVisibilityPrefChanged();
  // Called when the enterprise shortcuts policy may have changed.
  void OnEnterpriseShortcutsPolicyChanged();
  // Called when the NTP (re)loads. Sets mutable load time data.
  void OnLoad();

  // Called to maybe enable enterprise shortcuts visibility by default.
  void MaybeEnableEnterpriseShortcutsVisibility();

  // Based on the current profile and NTP promo controller, determine which
  // type of NTP promos can be shown, if any.
  std::string_view GetNtpPromoType();

  // Lazily creates and returns a reference to the owned contextual search
  // session handle for `realbox_handler_` and `composebox_handler_`.
  contextual_search::ContextualSearchSessionHandle*
  GetOrCreateContextualSessionHandle();

  // The counter for NewTabPage.Count UMA metrics.
  static int instance_count_;

  // Must outlive `realbox_handler_` and `composebox_handler_`.
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      shared_session_handle_;

  std::unique_ptr<NewTabPageHandler> page_handler_;
  mojo::Receiver<new_tab_page::mojom::PageHandlerFactory>
      page_factory_receiver_;
  std::unique_ptr<CustomizeButtonsHandler> customize_buttons_handler_;
  mojo::Receiver<customize_buttons::mojom::CustomizeButtonsHandlerFactory>
      customize_buttons_factory_receiver_;
  std::unique_ptr<MostVisitedHandler> most_visited_page_handler_;
  mojo::Receiver<most_visited::mojom::MostVisitedPageHandlerFactory>
      most_visited_page_factory_receiver_;
  std::unique_ptr<ComposeboxHandler> composebox_handler_;
  mojo::Receiver<composebox::mojom::PageHandlerFactory>
      composebox_page_factory_receiver_;
  std::unique_ptr<BrowserCommandHandler> promo_browser_command_handler_;
  mojo::Receiver<browser_command::mojom::CommandHandlerFactory>
      browser_command_factory_receiver_;
  std::unique_ptr<RealboxHandler> realbox_handler_;
  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_handler_factory_receiver_{this};
  std::unique_ptr<NtpPromoHandler> ntp_promo_handler_;
  mojo::Receiver<ntp_promo::mojom::NtpPromoHandlerFactory>
      ntp_promo_handler_factory_receiver_{this};
  std::unique_ptr<ActionChipsHandler> action_chips_handler_;
  mojo::Receiver<action_chips::mojom::ActionChipsHandlerFactory>
      action_chips_handler_factory_receiver_{this};
#if !defined(OFFICIAL_BUILD)
  std::unique_ptr<FooHandler> foo_handler_;
#endif
  std::unique_ptr<MostRelevantTabResumptionPageHandler>
      most_relevant_tab_resumption_handler_;
  std::unique_ptr<page_image_service::ImageServiceHandler>
      image_service_handler_;
  raw_ptr<Profile> profile_;
  raw_ptr<ThemeService> theme_service_;
  raw_ptr<NtpCustomBackgroundService> ntp_custom_background_service_;
  base::ScopedObservation<NtpCustomBackgroundService,
                          NtpCustomBackgroundServiceObserver>
      ntp_custom_background_service_observation_{this};
  // Time the NTP started loading. Used for logging the WebUI NTP's load
  // performance.
  base::Time navigation_start_time_;
  const std::vector<ntp::ModuleIdDetail> module_id_details_;

  // Mojo implementations for modules:
  std::unique_ptr<DriveSuggestionHandler> drive_handler_;
  std::unique_ptr<GoogleCalendarPageHandler> google_calendar_handler_;
  std::unique_ptr<MicrosoftAuthPageHandler> microsoft_auth_handler_;
  std::unique_ptr<MicrosoftFilesPageHandler> microsoft_files_handler_;
  std::unique_ptr<OutlookCalendarPageHandler> outlook_calendar_handler_;
  std::unique_ptr<TabGroupsPageHandler> tab_groups_handler_;
  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<NewTabPageUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_
