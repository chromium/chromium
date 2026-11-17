// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_suggestion.mojom.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/microsoft_files.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups.mojom.h"
#include "chrome/browser/ui/webui/customize_buttons/customize_buttons.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "components/themes/ntp_custom_background_service_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"
#include "ui/webui/resources/cr_components/most_visited/most_visited.mojom.h"
#include "ui/webui/resources/cr_components/search/plural_string_provider.mojom.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo.mojom.h"  // nogncheck
#include "components/user_education/webui/help_bubble_handler.h"  // nogncheck
#endif

#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/ui/webui/new_tab_page/foo/foo.mojom.h"  // nogncheck crbug.com/40147906
#endif

class GURL;
class NewTabPageMojoHandlers;
class NtpCustomBackgroundService;
class PrefRegistrySimple;
class PrefService;
class Profile;

namespace base {
class RefCountedMemory;
}  // namespace base

namespace content {
class BrowserContext;
class NavigationHandle;
class WebUI;
}  // namespace content

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

// WebUI host container for `chrome://new-tab-page`.
//
// Owns the WebUIDataSource (packaged resources, Content Security Policy, and
// loadTimeData computed via `ntp_load_time_data.h`) and delegates all Mojo
// interface bindings to `NewTabPageMojoHandlers`.
class NewTabPageUI : public ui::MojoWebUIController,
                     public NtpCustomBackgroundServiceObserver,
                     public content::WebContentsObserver {
 public:
  explicit NewTabPageUI(content::WebUI* web_ui);

  NewTabPageUI(const NewTabPageUI&) = delete;
  NewTabPageUI& operator=(const NewTabPageUI&) = delete;

  ~NewTabPageUI() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kRealboxContextualEntrypointElementId);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static void ResetProfilePrefs(PrefService* prefs);
  static bool IsManagedProfile(Profile* profile);
  static scoped_refptr<base::RefCountedMemory> GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

  void BindInterface(
      mojo::PendingReceiver<new_tab_page::mojom::PageHandlerFactory>
          pending_receiver);
  void BindInterface(
      mojo::PendingReceiver<searchbox::mojom::PageHandlerFactory>
          pending_receiver);
  void BindInterface(
      mojo::PendingReceiver<browser_command::mojom::CommandHandlerFactory>
          pending_receiver);
  void BindInterface(mojo::PendingReceiver<
                     customize_buttons::mojom::CustomizeButtonsHandlerFactory>
                         pending_receiver);
  void BindInterface(
      mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandlerFactory>
          pending_receiver);
  void BindInterface(
      mojo::PendingReceiver<file_suggestion::mojom::DriveSuggestionHandler>
          pending_receiver);
  void BindInterface(
      mojo::PendingReceiver<ntp::calendar::mojom::GoogleCalendarPageHandler>
          pending_receiver);
  void BindInterface(
      mojo::PendingReceiver<composebox::mojom::PageHandlerFactory>
          pending_receiver);
  void BindInterface(
      mojo::PendingReceiver<plural_string::mojom::PluralStringProvider>
          receiver);
#if !defined(OFFICIAL_BUILD)
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
#if !BUILDFLAG(IS_ANDROID)
  void BindInterface(mojo::PendingReceiver<ntp::tab_groups::mojom::PageHandler>
                         pending_page_handler);
  void BindInterface(
      mojo::PendingReceiver<ntp::calendar::mojom::OutlookCalendarPageHandler>
          pending_receiver);
  void BindInterface(
      mojo::PendingReceiver<file_suggestion::mojom::MicrosoftFilesPageHandler>
          pending_receiver);
  void BindInterface(
      mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandlerFactory>
          pending_receiver);
#endif  // !BUILDFLAG(IS_ANDROID)
  void BindInterface(
      mojo::PendingReceiver<action_chips::mojom::ActionChipsHandlerFactory>
          pending_receiver);

  void ConnectToParentDocument(
      mojo::PendingRemote<new_tab_page::mojom::MicrosoftAuthUntrustedDocument>
          child_page);

  contextual_search::ContextualSearchSessionHandle*
  GetOrCreateContextualSessionHandle();

 private:
  // NtpCustomBackgroundServiceObserver:
  void OnCustomBackgroundImageUpdated() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnColorProviderChanged() override;

  // Called when the NTP (re)loads. Sets mutable load time data.
  void OnLoad();

  raw_ptr<Profile> profile_;
  raw_ptr<NtpCustomBackgroundService> ntp_custom_background_service_;
  // Declared before `ntp_custom_background_service_observation_` so the
  // observation is reset before any Mojo handler is destroyed.
  std::unique_ptr<NewTabPageMojoHandlers> handlers_;
  base::ScopedObservation<NtpCustomBackgroundService,
                          NtpCustomBackgroundServiceObserver>
      ntp_custom_background_service_observation_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

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

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NEW_TAB_PAGE_UI_H_
