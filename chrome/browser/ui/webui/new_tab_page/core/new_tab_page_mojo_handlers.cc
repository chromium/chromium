// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/core/new_tab_page_mojo_handlers.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/new_tab_page/feature_promo_helper/new_tab_page_feature_promo_helper.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_suggestion_handler.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar_page_handler.h"
#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption_page_handler.h"
#include "chrome/browser/page_image_service/image_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/search_provider_logos/logo_service_factory.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/search/ntp_user_data_logger.h"
#include "chrome/browser/ui/webui/browser_command/browser_command_handler.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_handler.h"
#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_pref_observer.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/plural_string_provider.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/customize_buttons/customize_buttons_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_generator.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/page_image_service/image_service.h"
#include "components/page_image_service/image_service_handler.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/new_tab_page/modules/file_suggestion/microsoft_files_page_handler.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar_page_handler.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups_page_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo_handler.h"
#include "components/user_education/webui/help_bubble_handler.h"  // nogncheck
#endif  // !BUILDFLAG(IS_ANDROID)

#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/ui/webui/new_tab_page/foo/foo_handler.h"
#endif

// static
int NewTabPageMojoHandlers::instance_count_ = 0;

NewTabPageMojoHandlers::NewTabPageMojoHandlers(
    Profile* profile,
    content::WebContents* web_contents,
    TrackedElementHandlerGetter tracked_element_handler_getter)
    : profile_(profile),
      web_contents_(web_contents),
      tracked_element_handler_getter_(
          std::move(tracked_element_handler_getter)),
      module_id_details_(ntp::MakeModuleIdDetails(
          NewTabPageHandler::IsManagedProfile(profile_),
          profile_)) {
  instance_count_++;
  base::UmaHistogramCounts100("NewTabPage.Count", instance_count_);
  if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
    session_allows_drag_and_drop_ =
        session_handle->CheckSearchContentSharingSettings(profile_->GetPrefs());
  }
}

NewTabPageMojoHandlers::~NewTabPageMojoHandlers() {
  instance_count_--;
}

void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<new_tab_page::mojom::PageHandlerFactory>
        pending_receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }
  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<searchbox::mojom::PageHandlerFactory>
        pending_receiver) {
  if (searchbox_page_factory_receiver_.is_bound()) {
    searchbox_page_factory_receiver_.reset();
  }
  searchbox_page_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<composebox::mojom::PageHandlerFactory>
        pending_receiver) {
  auto* aim_service = AimEligibilityServiceFactory::GetForProfile(profile_);
  bool aim_eligible = aim_service && aim_service->IsAimEligible();

  if (!aim_eligible && !ntp_composebox::IsNtpComposeboxEnabled(profile_) &&
      !SearchboxHandler::
          GetVoiceSearchCoherenceAnySearchboxExperimentEnabled()) {
    return;
  }
  if (composebox_page_factory_receiver_.is_bound()) {
    composebox_page_factory_receiver_.reset();
  }
  composebox_page_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<action_chips::mojom::ActionChipsHandlerFactory>
        pending_receiver) {
  if (action_chips_handler_factory_receiver_.is_bound()) {
    action_chips_handler_factory_receiver_.reset();
  }
  action_chips_handler_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandlerFactory>
        pending_receiver) {
  if (most_visited_page_factory_receiver_.is_bound()) {
    most_visited_page_factory_receiver_.reset();
  }
  most_visited_page_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<
        customize_buttons::mojom::CustomizeButtonsHandlerFactory>
        pending_receiver) {
#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpCustomizeWebUiAndroid)) {
    return;
  }
#endif
  if (customize_buttons_factory_receiver_.is_bound()) {
    customize_buttons_factory_receiver_.reset();
  }
  customize_buttons_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        pending_receiver) {
  if (help_bubble_handler_factory_receiver_.is_bound()) {
    help_bubble_handler_factory_receiver_.reset();
  }
  help_bubble_handler_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<browser_command::mojom::CommandHandlerFactory>
        pending_receiver) {
  if (browser_command_factory_receiver_.is_bound()) {
    browser_command_factory_receiver_.reset();
  }
  browser_command_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<file_suggestion::mojom::DriveSuggestionHandler>
        pending_receiver) {
  drive_handler_ = std::make_unique<DriveSuggestionHandler>(
      std::move(pending_receiver), profile_);
}

void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<ntp::calendar::mojom::GoogleCalendarPageHandler>
        pending_receiver) {
  google_calendar_handler_ = std::make_unique<GoogleCalendarPageHandler>(
      std::move(pending_receiver), profile_);
}

void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<ntp::most_relevant_tab_resumption::mojom::PageHandler>
        pending_receiver) {
  most_relevant_tab_resumption_handler_ =
      std::make_unique<MostRelevantTabResumptionPageHandler>(
          std::move(pending_receiver), web_contents_);
}

void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<plural_string::mojom::PluralStringProvider>
        pending_receiver) {
  PluralStringProvider::MessageIdMap message_ids =
      GetSearchboxPluralStringIds();
  message_ids.emplace("modulesTabGroupsTabsText",
                      IDS_SAVED_TAB_GROUP_TABS_COUNT);
  PluralStringProvider::Create(std::move(pending_receiver),
                               std::move(message_ids));
}

void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<page_image_service::mojom::PageImageServiceHandler>
        pending_receiver) {
  base::WeakPtr<page_image_service::ImageService> image_service_weak;
  if (auto* image_service =
          page_image_service::ImageServiceFactory::GetForBrowserContext(
              profile_)) {
    image_service_weak = image_service->GetWeakPtr();
  }
  image_service_handler_ =
      std::make_unique<page_image_service::ImageServiceHandler>(
          std::move(pending_receiver), std::move(image_service_weak));
}

#if !BUILDFLAG(IS_ANDROID)
void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandlerFactory>
        pending_receiver) {
  if (ntp_promo_handler_factory_receiver_.is_bound()) {
    ntp_promo_handler_factory_receiver_.reset();
  }
  ntp_promo_handler_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<ntp::tab_groups::mojom::PageHandler>
        pending_receiver) {
  tab_groups_handler_ = std::make_unique<TabGroupsPageHandler>(
      std::move(pending_receiver), web_contents_);
}

void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<ntp::calendar::mojom::OutlookCalendarPageHandler>
        pending_receiver) {
  outlook_calendar_handler_ = std::make_unique<OutlookCalendarPageHandler>(
      std::move(pending_receiver), profile_);
}

void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<file_suggestion::mojom::MicrosoftFilesPageHandler>
        pending_receiver) {
  microsoft_files_handler_ = std::make_unique<MicrosoftFilesPageHandler>(
      std::move(pending_receiver), profile_);
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if !defined(OFFICIAL_BUILD)
void NewTabPageMojoHandlers::Bind(
    mojo::PendingReceiver<foo::mojom::FooHandler> pending_receiver) {
  foo_handler_ = std::make_unique<FooHandler>(std::move(pending_receiver));
}
#endif

void NewTabPageMojoHandlers::ConnectToParentDocument(
    mojo::PendingRemote<new_tab_page::mojom::MicrosoftAuthUntrustedDocument>
        child_page) {
  page_handler_->ConnectToParentDocument(std::move(child_page));
}

contextual_search::ContextualSearchSessionHandle*
NewTabPageMojoHandlers::GetOrCreateContextualSessionHandle() {
  if (!shared_session_handle_) {
    auto* contextual_search_service =
        ContextualSearchServiceFactory::GetForProfile(profile_);
    if (contextual_search_service) {
      shared_session_handle_ = contextual_search_service->CreateSession(
          ntp_composebox::CreateQueryControllerConfigParams(),
          contextual_search::ContextualSearchSource::kNewTabPage,
          lens::LensOverlayInvocationSource::kNtpContextualQuery);
      // TODO(crbug.com/469875247): Determine what to do with the return value
      // of this call, or move this call to a different location.
      shared_session_handle_->CheckSearchContentSharingSettings(
          profile_->GetPrefs());
    }
  }
  return shared_session_handle_.get();
}

void NewTabPageMojoHandlers::ClearContextualSessionHandle() {
  shared_session_handle_.reset();
}

void NewTabPageMojoHandlers::CreatePageHandler(
    mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
    mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
        pending_page_handler) {
  CHECK(pending_page.is_valid(), base::NotFatalUntil::M162);

  page_handler_ = std::make_unique<NewTabPageHandler>(
      std::move(pending_page_handler), std::move(pending_page), profile_,
      NtpCustomBackgroundServiceFactory::GetForProfile(profile_),
      ThemeServiceFactory::GetForProfile(profile_),
      LogoServiceFactory::GetForProfile(profile_),
      SyncServiceFactory::GetForProfile(profile_),
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          profile_),
      web_contents_, &module_id_details_);
}

void NewTabPageMojoHandlers::CreatePageHandler(
    mojo::PendingRemote<searchbox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler) {
  realbox_handler_ = std::make_unique<RealboxHandler>(
      std::move(pending_page_handler), std::move(pending_page), profile_,
      web_contents_,
      base::BindRepeating(
          &NewTabPageMojoHandlers::GetOrCreateContextualSessionHandle,
          base::Unretained(this)));
}

void NewTabPageMojoHandlers::CreatePageHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_page_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler) {
  composebox_handler_ = std::make_unique<ComposeboxHandler>(
      std::move(pending_page_handler), std::move(pending_searchbox_handler),
      std::move(pending_searchbox_page), profile_, web_contents_,
      base::BindRepeating(
          &NewTabPageMojoHandlers::GetOrCreateContextualSessionHandle,
          base::Unretained(this)),
      base::BindRepeating(&NewTabPageMojoHandlers::ClearContextualSessionHandle,
                          base::Unretained(this)));
  if (omnibox::IsDynamicAimFulfillmentEnabledForRealbox()) {
    composebox_handler_->OptInToDynamicAimFulfillment();
  }
}

void NewTabPageMojoHandlers::CreateActionChipsHandler(
    mojo::PendingReceiver<action_chips::mojom::ActionChipsHandler> handler,
    mojo::PendingRemote<action_chips::mojom::Page> page) {
  action_chips_handler_ = std::make_unique<ActionChipsHandler>(
      std::move(handler), std::move(page), profile_, web_contents_,
      std::make_unique<ActionChipsGeneratorImpl>(profile_),
      base::BindRepeating(
          &NewTabPageMojoHandlers::GetOrCreateContextualSessionHandle,
          base::Unretained(this)));
}

void NewTabPageMojoHandlers::CreatePageHandler(
    mojo::PendingRemote<most_visited::mojom::MostVisitedPage> pending_page,
    mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandler>
        pending_page_handler) {
  CHECK(pending_page.is_valid(), base::NotFatalUntil::M162);
  most_visited_page_handler_ = std::make_unique<MostVisitedHandler>(
      std::move(pending_page_handler), std::move(pending_page), profile_,
      web_contents_,
      std::make_unique<NTPUserDataLogger>(
          profile_, chrome::ChromeUINewTabPageURLAsGURL()));
  most_visited_pref_observer_ = std::make_unique<MostVisitedPrefObserver>(
      profile_, most_visited_page_handler_.get());
}

void NewTabPageMojoHandlers::CreateCustomizeButtonsHandler(
    mojo::PendingRemote<customize_buttons::mojom::CustomizeButtonsDocument>
        pending_page,
    mojo::PendingReceiver<customize_buttons::mojom::CustomizeButtonsHandler>
        pending_page_handler) {
  std::unique_ptr<NewTabPageFeaturePromoHelper> promo_helper;
#if !BUILDFLAG(IS_ANDROID)
  promo_helper = std::make_unique<NewTabPageFeaturePromoHelper>();
#endif
  customize_buttons_handler_ = std::make_unique<CustomizeButtonsHandler>(
      std::move(pending_page_handler), std::move(pending_page), web_contents_,
      webui::GetTabInterface(web_contents_), std::move(promo_helper));
}

void NewTabPageMojoHandlers::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler) {
// TODO(b/502297163): Implement for Android.
#if !BUILDFLAG(IS_ANDROID)
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(handler), std::move(client),
      tracked_element_handler_getter_ ? tracked_element_handler_getter_.Run()
                                      : nullptr);
#endif
}

#if !BUILDFLAG(IS_ANDROID)
void NewTabPageMojoHandlers::CreateNtpPromoHandler(
    mojo::PendingRemote<ntp_promo::mojom::NtpPromoClient> client,
    mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandler> handler) {
  ntp_promo_handler_ = std::make_unique<NtpPromoHandler>(
      std::move(client), std::move(handler), web_contents_);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void NewTabPageMojoHandlers::CreateBrowserCommandHandler(
    mojo::PendingReceiver<browser_command::mojom::CommandHandler>
        pending_handler) {
// TODO(b/502297163): Implement for Android.
#if !BUILDFLAG(IS_ANDROID)
  using browser_command::mojom::Command;
  std::vector<Command> supported_commands = {
      Command::kOpenSafetyCheck,
      Command::kOpenSafeBrowsingEnhancedProtectionSettings,
      Command::kNoOpCommand,
  };
  promo_browser_command_handler_ = std::make_unique<BrowserCommandHandler>(
      std::move(pending_handler), profile_, supported_commands, web_contents_);
#endif
}
