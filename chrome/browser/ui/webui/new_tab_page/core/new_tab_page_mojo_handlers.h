// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_CORE_NEW_TAB_PAGE_MOJO_HANDLERS_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_CORE_NEW_TAB_PAGE_MOJO_HANDLERS_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_suggestion.mojom.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/microsoft_files.mojom.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups.mojom.h"
#include "chrome/browser/ui/webui/customize_buttons/customize_buttons.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"
#include "ui/webui/resources/cr_components/most_visited/most_visited.mojom.h"
#include "ui/webui/resources/cr_components/search/plural_string_provider.mojom.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo.mojom.h"  // nogncheck
#endif

#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/ui/webui/new_tab_page/foo/foo.mojom.h"  // nogncheck crbug.com/40147906
#endif

class ActionChipsHandler;
class BrowserCommandHandler;
class ComposeboxHandler;
class CustomizeButtonsHandler;
class DriveSuggestionHandler;
class GoogleCalendarPageHandler;
class MostRelevantTabResumptionPageHandler;
class MostVisitedHandler;
class MostVisitedPrefObserver;
class NewTabPageHandler;
class Profile;
class RealboxHandler;
#if !BUILDFLAG(IS_ANDROID)
class MicrosoftFilesPageHandler;
class NtpPromoHandler;
class OutlookCalendarPageHandler;
class TabGroupsPageHandler;
#endif
#if !defined(OFFICIAL_BUILD)
class FooHandler;
#endif

namespace content {
class WebContents;
}  // namespace content

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

namespace page_image_service {
class ImageServiceHandler;
}  // namespace page_image_service

namespace ui {
class TrackedElementHandler;
}  // namespace ui

namespace user_education {
class HelpBubbleHandler;
}  // namespace user_education

// Creates and owns the browser-side Mojo handlers of a New Tab Page,
// independently of how the page is hosted (`chrome://new-tab-page` WebUI or
// the New Tab Page component extension).
class NewTabPageMojoHandlers
    : public new_tab_page::mojom::PageHandlerFactory,
      public searchbox::mojom::PageHandlerFactory,
      public composebox::mojom::PageHandlerFactory,
      public action_chips::mojom::ActionChipsHandlerFactory,
      public most_visited::mojom::MostVisitedPageHandlerFactory,
      public customize_buttons::mojom::CustomizeButtonsHandlerFactory,
      public help_bubble::mojom::HelpBubbleHandlerFactory,
#if !BUILDFLAG(IS_ANDROID)
      public ntp_promo::mojom::NtpPromoHandlerFactory,
#endif
      public browser_command::mojom::CommandHandlerFactory {
 public:
  using TrackedElementHandlerGetter =
      base::RepeatingCallback<base::WeakPtr<ui::TrackedElementHandler>()>;

  NewTabPageMojoHandlers(
      Profile* profile,
      content::WebContents* web_contents,
      TrackedElementHandlerGetter tracked_element_handler_getter);
  NewTabPageMojoHandlers(const NewTabPageMojoHandlers&) = delete;
  NewTabPageMojoHandlers& operator=(const NewTabPageMojoHandlers&) = delete;
  ~NewTabPageMojoHandlers() override;

  const std::vector<ntp::ModuleIdDetail>& module_id_details() const {
    return module_id_details_;
  }

  bool session_allows_drag_and_drop() const {
    return session_allows_drag_and_drop_;
  }

  // Factories. Rebinding replaces the previous receiver, e.g. when a reused
  // host loads a new document.
  void Bind(mojo::PendingReceiver<new_tab_page::mojom::PageHandlerFactory>
                pending_receiver);
  void Bind(mojo::PendingReceiver<searchbox::mojom::PageHandlerFactory>
                pending_receiver);
  void Bind(mojo::PendingReceiver<composebox::mojom::PageHandlerFactory>
                pending_receiver);
  void Bind(
      mojo::PendingReceiver<action_chips::mojom::ActionChipsHandlerFactory>
          pending_receiver);
  void Bind(
      mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandlerFactory>
          pending_receiver);
  void Bind(mojo::PendingReceiver<
            customize_buttons::mojom::CustomizeButtonsHandlerFactory>
                pending_receiver);
  void Bind(mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
                pending_receiver);
  void Bind(mojo::PendingReceiver<browser_command::mojom::CommandHandlerFactory>
                pending_receiver);

  // Handlers bound directly.
  void Bind(
      mojo::PendingReceiver<file_suggestion::mojom::DriveSuggestionHandler>
          pending_receiver);
  void Bind(
      mojo::PendingReceiver<ntp::calendar::mojom::GoogleCalendarPageHandler>
          pending_receiver);
  void
  Bind(mojo::PendingReceiver<
       ntp::most_relevant_tab_resumption::mojom::PageHandler> pending_receiver);
  void Bind(mojo::PendingReceiver<plural_string::mojom::PluralStringProvider>
                pending_receiver);
  void Bind(
      mojo::PendingReceiver<page_image_service::mojom::PageImageServiceHandler>
          pending_receiver);
#if !BUILDFLAG(IS_ANDROID)
  void Bind(mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandlerFactory>
                pending_receiver);
  void Bind(mojo::PendingReceiver<ntp::tab_groups::mojom::PageHandler>
                pending_receiver);
  void Bind(
      mojo::PendingReceiver<ntp::calendar::mojom::OutlookCalendarPageHandler>
          pending_receiver);
  void Bind(
      mojo::PendingReceiver<file_suggestion::mojom::MicrosoftFilesPageHandler>
          pending_receiver);
#endif  // !BUILDFLAG(IS_ANDROID)
#if !defined(OFFICIAL_BUILD)
  void Bind(mojo::PendingReceiver<foo::mojom::FooHandler> pending_receiver);
#endif

  // Connects the chrome-untrusted://ntp-microsoft-auth child document to the
  // page handler.
  void ConnectToParentDocument(
      mojo::PendingRemote<new_tab_page::mojom::MicrosoftAuthUntrustedDocument>
          child_page);

  // Returns the contextual search session shared by the searchbox, composebox
  // and action chips of this page, creating it if needed.
  contextual_search::ContextualSearchSessionHandle*
  GetOrCreateContextualSessionHandle();

 private:
  // new_tab_page::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
      mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
          pending_page_handler) override;

  // searchbox::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<searchbox::mojom::Page> pending_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler)
      override;

  // composebox::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<composebox::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler) override;

  // action_chips::mojom::ActionChipsHandlerFactory:
  void CreateActionChipsHandler(
      mojo::PendingReceiver<action_chips::mojom::ActionChipsHandler> handler,
      mojo::PendingRemote<action_chips::mojom::Page> page) override;

  // most_visited::mojom::MostVisitedPageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<most_visited::mojom::MostVisitedPage> pending_page,
      mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandler>
          pending_page_handler) override;

  // customize_buttons::mojom::CustomizeButtonsHandlerFactory:
  void CreateCustomizeButtonsHandler(
      mojo::PendingRemote<customize_buttons::mojom::CustomizeButtonsDocument>
          pending_page,
      mojo::PendingReceiver<customize_buttons::mojom::CustomizeButtonsHandler>
          pending_page_handler) override;

  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

#if !BUILDFLAG(IS_ANDROID)
  // ntp_promo::mojom::NtpPromoHandlerFactory:
  void CreateNtpPromoHandler(
      mojo::PendingRemote<ntp_promo::mojom::NtpPromoClient> client,
      mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandler> handler)
      override;
#endif  // !BUILDFLAG(IS_ANDROID)

  // browser_command::mojom::CommandHandlerFactory:
  void CreateBrowserCommandHandler(
      mojo::PendingReceiver<browser_command::mojom::CommandHandler>
          pending_handler) override;

  void ClearContextualSessionHandle();

  // The number of live New Tab Pages, for the NewTabPage.Count histogram.
  static int instance_count_;

  const raw_ptr<Profile> profile_;
  const raw_ptr<content::WebContents> web_contents_;
  TrackedElementHandlerGetter tracked_element_handler_getter_;
  bool session_allows_drag_and_drop_ = false;

  // The members below are declared in the same order as they were in
  // `NewTabPageUI` before they moved here, so that they are destroyed in the
  // same order as before.

  // Must outlive `realbox_handler_` and `composebox_handler_`.
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      shared_session_handle_;

  std::unique_ptr<NewTabPageHandler> page_handler_;
  mojo::Receiver<new_tab_page::mojom::PageHandlerFactory>
      page_factory_receiver_{this};
  std::unique_ptr<CustomizeButtonsHandler> customize_buttons_handler_;
  mojo::Receiver<customize_buttons::mojom::CustomizeButtonsHandlerFactory>
      customize_buttons_factory_receiver_{this};
  std::unique_ptr<MostVisitedHandler> most_visited_page_handler_;
  std::unique_ptr<MostVisitedPrefObserver> most_visited_pref_observer_;
  mojo::Receiver<most_visited::mojom::MostVisitedPageHandlerFactory>
      most_visited_page_factory_receiver_{this};
  mojo::Receiver<composebox::mojom::PageHandlerFactory>
      composebox_page_factory_receiver_{this};
  std::unique_ptr<ComposeboxHandler> composebox_handler_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
  std::unique_ptr<NtpPromoHandler> ntp_promo_handler_;
  mojo::Receiver<ntp_promo::mojom::NtpPromoHandlerFactory>
      ntp_promo_handler_factory_receiver_{this};
  std::unique_ptr<MicrosoftFilesPageHandler> microsoft_files_handler_;
  std::unique_ptr<OutlookCalendarPageHandler> outlook_calendar_handler_;
  std::unique_ptr<TabGroupsPageHandler> tab_groups_handler_;
#endif  // !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<ActionChipsHandler> action_chips_handler_;
  mojo::Receiver<action_chips::mojom::ActionChipsHandlerFactory>
      action_chips_handler_factory_receiver_{this};
  std::unique_ptr<BrowserCommandHandler> promo_browser_command_handler_;
  mojo::Receiver<browser_command::mojom::CommandHandlerFactory>
      browser_command_factory_receiver_{this};
  std::unique_ptr<RealboxHandler> realbox_handler_;
  mojo::Receiver<searchbox::mojom::PageHandlerFactory>
      searchbox_page_factory_receiver_{this};
  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_handler_factory_receiver_{this};
#if !defined(OFFICIAL_BUILD)
  std::unique_ptr<FooHandler> foo_handler_;
#endif
  std::unique_ptr<MostRelevantTabResumptionPageHandler>
      most_relevant_tab_resumption_handler_;
  std::unique_ptr<page_image_service::ImageServiceHandler>
      image_service_handler_;
  const std::vector<ntp::ModuleIdDetail> module_id_details_;

  // Mojo implementations for modules:
  std::unique_ptr<DriveSuggestionHandler> drive_handler_;
  std::unique_ptr<GoogleCalendarPageHandler> google_calendar_handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_CORE_NEW_TAB_PAGE_MOJO_HANDLERS_H_
