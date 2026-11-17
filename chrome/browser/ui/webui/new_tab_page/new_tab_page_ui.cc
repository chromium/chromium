// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_pref_observer.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_metrics.h"
#include "chrome/browser/ui/webui/new_tab_page/core/new_tab_page_mojo_handlers.h"
#include "chrome/browser/ui/webui/new_tab_page/core/ntp_load_time_data.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/page_not_available_for_guest/page_not_available_for_guest_ui.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "chrome/grit/new_tab_page_resources_map.h"
#include "chrome/grit/new_tab_shared_resources.h"
#include "chrome/grit/new_tab_shared_resources_map.h"
#include "chrome/grit/theme_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/tracked_element/tracked_element_handler_document_singleton.h"
#include "ui/webui/webui_util.h"
#include "url/gurl.h"

bool NewTabPageUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return !profile->IsOffTheRecord();
}

std::unique_ptr<content::WebUIController>
NewTabPageUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                          const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  if (profile->IsGuestSession()) {
    return std::make_unique<PageNotAvailableForGuestUI>(
        web_ui, chrome::kChromeUINewTabPageHost);
  }
  return std::make_unique<NewTabPageUI>(web_ui);
}

namespace {

constexpr char kPrevNavigationTimePrefName[] = "NewTabPage.PrevNavigationTime";

bool HasCredentials(Profile* profile) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  return
      /* Can be null if Chrome signin is disabled. */ identity_manager &&
      !identity_manager->GetAccountsInCookieJar()
           .GetPotentiallyInvalidSignedInAccounts()
           .empty();
}

content::WebUIDataSource* CreateAndAddNewTabPageUiHtmlSource(
    Profile* profile,
    bool session_allows_drag_and_drop) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUINewTabPageHost);

  if (!ntp::AreActionChipsEnabled(profile)) {
    action_chips::RecordActionChipsAnyShown(false);
  }
  source->AddLocalizedStrings(
      ntp::BuildLoadTimeData(profile, session_allows_drag_and_drop));

  webui::SetupWebUIDataSource(source, kNewTabPageResources,
                              IDR_NEW_TAB_PAGE_NEW_TAB_PAGE_HTML);

#if BUILDFLAG(OPTIMIZE_WEBUI)
  // By default, `SetupWebUIDataSource` maps `new_tab_page.js` to the bundle
  // that defers modules like ntp composebox to `lazy_load.js`. When
  // `kNtpLazyLoadComposebox` is disabled, remap all three Rollup chunks to the
  // eager bundle built from `new_tab_page_eager.ts`, which eagerly includes
  // those modules.
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpLazyLoadComposebox)) {
    source->AddResourcePath(
        "new_tab_page.js", IDR_NEW_TAB_PAGE_EAGER_NEW_TAB_PAGE_EAGER_ROLLUP_JS);
    source->AddResourcePath("lazy_load.js",
                            IDR_NEW_TAB_PAGE_EAGER_LAZY_LOAD_ROLLUP_JS);
    source->AddResourcePath("shared.rollup.js",
                            IDR_NEW_TAB_PAGE_EAGER_SHARED_ROLLUP_JS);
  }
#endif

  source->AddResourcePaths(kNewTabSharedResources);

  // Allow embedding of iframes for the doodle and
  // chrome-untrusted://new-tab-page for other external content and resources.
  // NOTE: Use caution when overriding content security policies as that cean
  // lead to subtle security bugs such as https://crbug.com/40057334.
  GURL google_base_url = GURL(TemplateURLServiceFactory::GetForProfile(profile)
                                  ->search_terms_data()
                                  .GoogleBaseURLValue());
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      base::StringPrintf("child-src https: %s %s %s %s;",
                         google_util::CommandLineGoogleBaseURL().spec().c_str(),
                         google_base_url.spec().c_str(),
                         chrome::kChromeUIUntrustedNewTabPageUrl,
                         chrome::kChromeUIUntrustedNtpMicrosoftAuthURL));
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::MediaSrc,
      "media-src blob: data: 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      base::StrCat(
          {webui::kDefaultTrustedTypesPolicies, " composebox-input;"}));

  return source;
}

}  // namespace

NewTabPageUI::NewTabPageUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui,
                              /*enable_chrome_send=*/true,
                              /*enable_chrome_histograms=*/true),
      content::WebContentsObserver(web_ui->GetWebContents()),
      profile_(Profile::FromWebUI(web_ui)),
      ntp_custom_background_service_(
          NtpCustomBackgroundServiceFactory::GetForProfile(profile_)),
      handlers_(std::make_unique<NewTabPageMojoHandlers>(
          profile_,
          web_contents(),
          base::BindRepeating(
              [](content::WebUI* web_ui) {
                return ui::TrackedElementHandlerDocumentSingleton::GetOrCreate(
                    web_ui->GetRenderFrameHost());
              },
              base::Unretained(web_ui)))) {
  CreateAndAddNewTabPageUiHtmlSource(profile_,
                                     handlers_->session_allows_drag_and_drop());
  ntp::AddDataSources(profile_);

  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

  ntp::AddThirdPartyCookieGrants(profile_);

// TODO(b/502297163): Implement for Android.
#if !BUILDFLAG(IS_ANDROID)
  // Store basic theme info in load time data to make the background color and
  // background image available as soon as the page loads to prevent a potential
  // white flicker.

  ntp_custom_background_service_observation_.Observe(
      ntp_custom_background_service_.get());
#endif

  // Populates the load time data with basic info.
  OnColorProviderChanged();
  OnCustomBackgroundImageUpdated();
  OnLoad();

  ui::TrackedElementHandlerDocumentSingleton::Register(
      this, NewTabPageHandler::GetTrackedElementIds());
}

WEB_UI_CONTROLLER_TYPE_IMPL(NewTabPageUI)

NewTabPageUI::~NewTabPageUI() = default;

// static
void NewTabPageUI::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kPrevNavigationTimePrefName, base::Time());
  registry->RegisterTimePref(ntp_prefs::kNtpLastModuleStalenessUpdate,
                             base::Time());
  registry->RegisterDictionaryPref(ntp_prefs::kNtpModuleStalenessCountDict);
  registry->RegisterDictionaryPref(
      ntp_prefs::kNtpModulesAutoRemovalDisabledDict);
  registry->RegisterBooleanPref(ntp_prefs::kNtpAnimatedDoodlesEnabled, true);
  registry->RegisterBooleanPref(ntp_prefs::kNtpDoodleMuralsEnabled, true);
}

// static
void NewTabPageUI::ResetProfilePrefs(PrefService* prefs) {
  MostVisitedPrefObserver::ResetProfilePrefs(prefs);
  prefs->SetTime(ntp_prefs::kNtpLastModuleStalenessUpdate, base::Time());
  prefs->SetDict(ntp_prefs::kNtpModuleStalenessCountDict, base::DictValue());
  prefs->SetDict(ntp_prefs::kNtpModulesAutoRemovalDisabledDict,
                 base::DictValue());
  prefs->SetBoolean(ntp_prefs::kNtpAnimatedDoodlesEnabled, true);
  prefs->SetBoolean(ntp_prefs::kNtpDoodleMuralsEnabled, true);
  prefs->SetDict(prefs::kContextMenuAnimationState, base::DictValue());
}

// static
scoped_refptr<base::RefCountedMemory> NewTabPageUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_NTP_FAVICON, scale_factor);
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<new_tab_page::mojom::PageHandlerFactory>
        pending_receiver) {
  handlers_->Bind(std::move(pending_receiver));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<searchbox::mojom::PageHandlerFactory>
        pending_receiver) {
  handlers_->Bind(std::move(pending_receiver));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<browser_command::mojom::CommandHandlerFactory>
        pending_receiver) {
  handlers_->Bind(std::move(pending_receiver));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<
        customize_buttons::mojom::CustomizeButtonsHandlerFactory>
        pending_receiver) {
  handlers_->Bind(std::move(pending_receiver));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandlerFactory>
        pending_receiver) {
  handlers_->Bind(std::move(pending_receiver));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<file_suggestion::mojom::DriveSuggestionHandler>
        pending_receiver) {
  handlers_->Bind(std::move(pending_receiver));
}

#if !defined(OFFICIAL_BUILD)
void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<foo::mojom::FooHandler> pending_page_handler) {
  handlers_->Bind(std::move(pending_page_handler));
}
#endif

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<ntp::most_relevant_tab_resumption::mojom::PageHandler>
        pending_page_handler) {
  handlers_->Bind(std::move(pending_page_handler));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<ntp::calendar::mojom::GoogleCalendarPageHandler>
        pending_page_handler) {
  handlers_->Bind(std::move(pending_page_handler));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<plural_string::mojom::PluralStringProvider>
        receiver) {
  handlers_->Bind(std::move(receiver));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<composebox::mojom::PageHandlerFactory>
        pending_receiver) {
  handlers_->Bind(std::move(pending_receiver));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<page_image_service::mojom::PageImageServiceHandler>
        pending_page_handler) {
  handlers_->Bind(std::move(pending_page_handler));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        pending_receiver) {
  handlers_->Bind(std::move(pending_receiver));
}

#if !BUILDFLAG(IS_ANDROID)
void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<ntp::tab_groups::mojom::PageHandler>
        pending_page_handler) {
  handlers_->Bind(std::move(pending_page_handler));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<ntp::calendar::mojom::OutlookCalendarPageHandler>
        pending_page_handler) {
  handlers_->Bind(std::move(pending_page_handler));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<file_suggestion::mojom::MicrosoftFilesPageHandler>
        pending_page_handler) {
  handlers_->Bind(std::move(pending_page_handler));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandlerFactory>
        pending_receiver) {
  handlers_->Bind(std::move(pending_receiver));
}
#endif  // !BUILDFLAG(IS_ANDROID)

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<action_chips::mojom::ActionChipsHandlerFactory>
        pending_receiver) {
  handlers_->Bind(std::move(pending_receiver));
}

void NewTabPageUI::ConnectToParentDocument(
    mojo::PendingRemote<new_tab_page::mojom::MicrosoftAuthUntrustedDocument>
        child_page) {
  handlers_->ConnectToParentDocument(std::move(child_page));
}

contextual_search::ContextualSearchSessionHandle*
NewTabPageUI::GetOrCreateContextualSessionHandle() {
  return handlers_->GetOrCreateContextualSessionHandle();
}

// OnColorProviderChanged can be called during the destruction process and
// should not directly access any member variables.
void NewTabPageUI::OnColorProviderChanged() {
  if (!web_contents() || !web_ui()) {
    return;
  }
  content::WebUIDataSource::Update(
      Profile::FromWebUI(web_ui()), chrome::kChromeUINewTabPageHost,
      ntp::BuildBackgroundColorLoadTimeData(web_contents()));
}

void NewTabPageUI::OnCustomBackgroundImageUpdated() {
  content::WebUIDataSource::Update(
      profile_, chrome::kChromeUINewTabPageHost,
      ntp::BuildCustomBackgroundImageLoadTimeData(profile_));
}

void NewTabPageUI::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->GetURL() == chrome::ChromeUINewTabPageURLAsGURL()) {
    const base::Time now = base::Time::Now();
    OnLoad();

    auto prev_navigation_time =
        profile_->GetPrefs()->GetTime(kPrevNavigationTimePrefName);
    if (!prev_navigation_time.is_null()) {
      base::UmaHistogramCustomTimes("NewTabPage.TimeSinceLastNTP",
                                    now - prev_navigation_time,
                                    base::Seconds(1), base::Days(1), 100);
    }
    profile_->GetPrefs()->SetTime(kPrevNavigationTimePrefName, now);
    base::UmaHistogramBoolean("NewTabPage.HasCredentials",
                              HasCredentials(profile_));
  }
}

void NewTabPageUI::OnLoad() {
  content::WebUIDataSource::Update(
      profile_, chrome::kChromeUINewTabPageHost,
      ntp::BuildNavigationLoadTimeData(profile_, web_contents(),
                                       handlers_->module_id_details()));
}
