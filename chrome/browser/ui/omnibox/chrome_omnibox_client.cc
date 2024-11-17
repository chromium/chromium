// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/typed_macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/feedback/public/feedback_source.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/omnibox/autocomplete_controller_emitter_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ssl/typed_navigation_upgrade_throttle.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_navigation_observer.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_service.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#endif

using predictors::AutocompleteActionPredictor;

ChromeOmniboxClient::ChromeOmniboxClient(LocationBar* location_bar,
                                         Browser* browser,
                                         Profile* profile)
    : location_bar_(location_bar),
      browser_(browser),
      profile_(profile),
      scheme_classifier_(
          std::make_unique<ChromeAutocompleteSchemeClassifier>(profile)),
      favicon_cache_(FaviconServiceFactory::GetForProfile(
                         profile,
                         ServiceAccessType::EXPLICIT_ACCESS),
                     HistoryServiceFactory::GetForProfile(
                         profile,
                         ServiceAccessType::EXPLICIT_ACCESS)) {}

ChromeOmniboxClient::~ChromeOmniboxClient() {
  BitmapFetcherService* bitmap_fetcher_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(profile_);
  for (auto request_id : request_ids_) {
    bitmap_fetcher_service->CancelRequest(request_id);
  }
}

std::unique_ptr<AutocompleteProviderClient>
ChromeOmniboxClient::CreateAutocompleteProviderClient() {
  return std::make_unique<ChromeAutocompleteProviderClient>(profile_);
}

bool ChromeOmniboxClient::CurrentPageExists() const {
  return (location_bar_->GetWebContents() != nullptr);
}

const GURL& ChromeOmniboxClient::GetURL() const {
  return CurrentPageExists() ? location_bar_->GetWebContents()->GetVisibleURL()
                             : GURL::EmptyGURL();
}

const std::u16string& ChromeOmniboxClient::GetTitle() const {
  return CurrentPageExists() ? location_bar_->GetWebContents()->GetTitle()
                             : base::EmptyString16();
}

gfx::Image ChromeOmniboxClient::GetFavicon() const {
  return favicon::ContentFaviconDriver::FromWebContents(
             location_bar_->GetWebContents())
      ->GetFavicon();
}

ukm::SourceId ChromeOmniboxClient::GetUKMSourceId() const {
  return CurrentPageExists() ? location_bar_->GetWebContents()
                                   ->GetPrimaryMainFrame()
                                   ->GetPageUkmSourceId()
                             : ukm::kInvalidSourceId;
}

bool ChromeOmniboxClient::IsLoading() const {
  return location_bar_->GetWebContents()->IsLoading();
}

bool ChromeOmniboxClient::IsPasteAndGoEnabled() const {
  return location_bar_->command_updater()->IsCommandEnabled(
      IDC_OPEN_CURRENT_URL);
}

bool ChromeOmniboxClient::IsDefaultSearchProviderEnabled() const {
  const base::Value::Dict& url_dict = profile_->GetPrefs()->GetDict(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
  return !url_dict.FindBool(DefaultSearchManager::kDisabledByPolicy)
              .value_or(false);
}

SessionID ChromeOmniboxClient::GetSessionID() const {
  return sessions::SessionTabHelper::IdForTab(location_bar_->GetWebContents());
}

PrefService* ChromeOmniboxClient::GetPrefs() {
  return profile_->GetPrefs();
}

const PrefService* ChromeOmniboxClient::GetPrefs() const {
  return profile_->GetPrefs();
}

bookmarks::BookmarkModel* ChromeOmniboxClient::GetBookmarkModel() {
  return BookmarkModelFactory::GetForBrowserContext(profile_);
}

AutocompleteControllerEmitter*
ChromeOmniboxClient::GetAutocompleteControllerEmitter() {
  return AutocompleteControllerEmitterFactory::GetForBrowserContext(profile_);
}

TemplateURLService* ChromeOmniboxClient::GetTemplateURLService() {
  return TemplateURLServiceFactory::GetForProfile(profile_);
}

const AutocompleteSchemeClassifier& ChromeOmniboxClient::GetSchemeClassifier()
    const {
  return *scheme_classifier_;
}

AutocompleteClassifier* ChromeOmniboxClient::GetAutocompleteClassifier() {
  return AutocompleteClassifierFactory::GetForProfile(profile_);
}

bool ChromeOmniboxClient::ShouldDefaultTypedNavigationsToHttps() const {
  return base::FeatureList::IsEnabled(omnibox::kDefaultTypedNavigationsToHttps);
}

int ChromeOmniboxClient::GetHttpsPortForTesting() const {
  return TypedNavigationUpgradeThrottle::GetHttpsPortForTesting();
}

bool ChromeOmniboxClient::IsUsingFakeHttpsForHttpsUpgradeTesting() const {
  // Tests on desktop/Android always use a real HTTPS server.
  return false;
}

gfx::Image ChromeOmniboxClient::GetIconIfExtensionMatch(
    const AutocompleteMatch& match) const {
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* template_url = match.GetTemplateURL(service, false);
  if (template_url &&
      (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION)) {
    return extensions::OmniboxAPI::Get(profile_)->GetOmniboxIcon(
        template_url->GetExtensionId());
  }
  return gfx::Image();
}

gfx::Image ChromeOmniboxClient::GetSizedIcon(
    const gfx::VectorIcon& vector_icon_type,
    SkColor vector_icon_color) const {
  return gfx::Image(gfx::CreateVectorIcon(
      vector_icon_type, GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
      vector_icon_color));
}

gfx::Image ChromeOmniboxClient::GetSizedIcon(const gfx::Image& icon) const {
  if (icon.IsEmpty())
    return icon;

  const int icon_size = GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
  // In touch mode, icons are 20x20. FaviconCache and ExtensionIconManager both
  // guarantee favicons and extension icons will be 16x16, so add extra padding
  // around them to align them vertically with the other vector icons.
  DCHECK_GE(icon_size, icon.Height());
  DCHECK_GE(icon_size, icon.Width());
  auto padding_border = gfx::Insets::VH((icon_size - icon.Height()) / 2,
                                        (icon_size - icon.Width()) / 2);
  if (!padding_border.IsEmpty()) {
    return gfx::Image(gfx::CanvasImageSource::CreatePadded(*icon.ToImageSkia(),
                                                           padding_border));
  }
  return icon;
}

std::u16string ChromeOmniboxClient::GetFormattedFullURL() const {
  return location_bar_->GetLocationBarModel()->GetFormattedFullURL();
}

std::u16string ChromeOmniboxClient::GetURLForDisplay() const {
  return location_bar_->GetLocationBarModel()->GetURLForDisplay();
}

GURL ChromeOmniboxClient::GetNavigationEntryURL() const {
  return location_bar_->GetLocationBarModel()->GetURL();
}

metrics::OmniboxEventProto::PageClassification
ChromeOmniboxClient::GetPageClassification(bool is_prefetch) const {
  return location_bar_->GetLocationBarModel()->GetPageClassification(
      is_prefetch);
}

security_state::SecurityLevel ChromeOmniboxClient::GetSecurityLevel() const {
  return location_bar_->GetLocationBarModel()->GetSecurityLevel();
}

net::CertStatus ChromeOmniboxClient::GetCertStatus() const {
  return location_bar_->GetLocationBarModel()->GetCertStatus();
}

const gfx::VectorIcon& ChromeOmniboxClient::GetVectorIcon() const {
  return location_bar_->GetLocationBarModel()->GetVectorIcon();
}

bool ChromeOmniboxClient::ProcessExtensionKeyword(
    const std::u16string& text,
    const TemplateURL* template_url,
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition) {
  if (template_url->type() != TemplateURL::OMNIBOX_API_EXTENSION)
    return false;

  // Strip the keyword + leading space off the input, but don't exceed
  // fill_into_edit.  An obvious case is that the user may not have entered
  // a leading space and is asking to launch this extension without any
  // additional input.
  size_t prefix_length =
      std::min(match.keyword.length() + 1, match.fill_into_edit.length());
  extensions::ExtensionOmniboxEventRouter::OnInputEntered(
      location_bar_->GetWebContents(), template_url->GetExtensionId(),
      base::UTF16ToUTF8(match.fill_into_edit.substr(prefix_length)),
      disposition);

  OnSuccessfulNavigation(profile_, text, match);
  return true;
}

void ChromeOmniboxClient::OnInputStateChanged() {
  if (!location_bar_->GetWebContents()) {
    return;
  }
  if (auto* helper =
          OmniboxTabHelper::FromWebContents(location_bar_->GetWebContents())) {
    helper->OnInputStateChanged();
  }
}

void ChromeOmniboxClient::OnFocusChanged(OmniboxFocusState state,
                                         OmniboxFocusChangeReason reason) {
  if (!location_bar_->GetWebContents()) {
    return;
  }
  if (auto* helper =
          OmniboxTabHelper::FromWebContents(location_bar_->GetWebContents())) {
    helper->OnFocusChanged(state, reason);
  }
}

void ChromeOmniboxClient::OnResultChanged(
    const AutocompleteResult& result,
    bool default_match_changed,
    bool should_preload,
    const BitmapFetchedCallback& on_bitmap_fetched) {
  if (should_preload) {
    if (SearchPrefetchService* search_prefetch_service =
            SearchPrefetchServiceFactory::GetForProfile(profile_)) {
      search_prefetch_service->OnResultChanged(location_bar_->GetWebContents(),
                                               result);
    }
  }

  BitmapFetcherService* bitmap_fetcher_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(profile_);

  // Clear out the old requests.
  for (auto request_id : request_ids_) {
    bitmap_fetcher_service->CancelRequest(request_id);
  }
  request_ids_.clear();
  // Create new requests.
  int result_index = -1;
  for (const AutocompleteMatch& match : result) {
    ++result_index;
    if (match.ImageUrl().is_empty()) {
      continue;
    }

    request_ids_.push_back(bitmap_fetcher_service->RequestImage(
        match.ImageUrl(), base::BindOnce(&ChromeOmniboxClient::OnBitmapFetched,
                                         weak_factory_.GetWeakPtr(),
                                         on_bitmap_fetched, result_index)));
  }
}

gfx::Image ChromeOmniboxClient::GetFaviconForPageUrl(
    const GURL& page_url,
    FaviconFetchedCallback on_favicon_fetched) {
  return favicon_cache_.GetFaviconForPageUrl(page_url,
                                             std::move(on_favicon_fetched));
}

gfx::Image ChromeOmniboxClient::GetFaviconForDefaultSearchProvider(
    FaviconFetchedCallback on_favicon_fetched) {
  const TemplateURL* const default_provider =
      GetTemplateURLService()->GetDefaultSearchProvider();
  if (!default_provider)
    return gfx::Image();

  return favicon_cache_.GetFaviconForIconUrl(default_provider->favicon_url(),
                                             std::move(on_favicon_fetched));
}

gfx::Image ChromeOmniboxClient::GetFaviconForKeywordSearchProvider(
    const TemplateURL* template_url,
    FaviconFetchedCallback on_favicon_fetched) {
  if (!template_url)
    return gfx::Image();

  return favicon_cache_.GetFaviconForIconUrl(template_url->favicon_url(),
                                             std::move(on_favicon_fetched));
}

void ChromeOmniboxClient::OnTextChanged(const AutocompleteMatch& current_match,
                                        bool user_input_in_progress,
                                        const std::u16string& user_text,
                                        const AutocompleteResult& result,
                                        bool has_focus) {
  AutocompleteActionPredictor::Action recommended_action =
      AutocompleteActionPredictor::ACTION_NONE;
  if (user_input_in_progress) {
    content::WebContents* web_contents = location_bar_->GetWebContents();
    AutocompleteActionPredictor* action_predictor =
        predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_);
    action_predictor->RegisterTransitionalMatches(user_text, result);
    // Confer with the AutocompleteActionPredictor to determine what action,
    // if any, we should take. Get the recommended action here even if we
    // don't need it so we can get stats for anyone who is opted in to UMA,
    // but only get it if the user has actually typed something to avoid
    // constructing it before it's needed. Note: This event is triggered as
    // part of startup when the initial tab transitions to the start page.
    recommended_action = action_predictor->RecommendAction(
        user_text, current_match, web_contents);
  }

  switch (recommended_action) {
    case AutocompleteActionPredictor::ACTION_PRERENDER:
      // It's possible that there is no current page, for instance if the tab
      // has been closed or on return from a sleep state.
      // (http://crbug.com/105689)
      if (!CurrentPageExists())
        break;
      // Ask for prerendering if the destination URL is different than the
      // current URL.
      if (current_match.destination_url != GetURL())
        DoPrerender(current_match);
      break;
    case AutocompleteActionPredictor::ACTION_PRECONNECT:
      DoPreconnect(current_match);
      break;
    case AutocompleteActionPredictor::ACTION_NONE:
      break;
  }

  location_bar_->OnChanged();
}

void ChromeOmniboxClient::OnRevert() {
  AutocompleteActionPredictor* action_predictor =
      predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_);
  action_predictor->UpdateDatabaseFromTransitionalMatches(GURL());
}

void ChromeOmniboxClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  // Record the value if prerender for search suggestion was not started. Other
  // values (kHitFinished, kUnused, kCancelled) are recorded in
  // PrerenderManager.
  content::WebContents* web_contents = location_bar_->GetWebContents();
  if (web_contents) {
    if (SearchPrefetchService* search_prefetch_service =
            SearchPrefetchServiceFactory::GetForProfile(profile_)) {
      search_prefetch_service->OnURLOpenedFromOmnibox(log);
    }

    auto* prerender_manager = PrerenderManager::FromWebContents(web_contents);
    if (!prerender_manager ||
        !prerender_manager->HasSearchResultPagePrerendered()) {
      base::UmaHistogramEnumeration(
          internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
          PrerenderPredictionStatus::kNotStarted);
    }
  }

  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_)
      ->OnOmniboxOpenedUrl(*log);
}

void ChromeOmniboxClient::OnBookmarkLaunched() {
  RecordBookmarkLaunch(BookmarkLaunchLocation::kOmnibox,
                       profile_metrics::GetBrowserProfileType(profile_));
}

void ChromeOmniboxClient::DiscardNonCommittedNavigations() {
  location_bar_->GetWebContents()->GetController().DiscardNonCommittedEntries();
}

void ChromeOmniboxClient::FocusWebContents() {
  if (location_bar_->GetWebContents()) {
    location_bar_->GetWebContents()->Focus();
  }
}

void ChromeOmniboxClient::OnNavigationLikely(
    size_t index,
    const AutocompleteMatch& match,
    omnibox::mojom::NavigationPredictor navigation_predictor) {
  if (SearchPrefetchService* search_prefetch_service =
          SearchPrefetchServiceFactory::GetForProfile(profile_)) {
    search_prefetch_service->OnNavigationLikely(
        index, match, navigation_predictor, location_bar_->GetWebContents());
  }
}

void ChromeOmniboxClient::ShowFeedbackPage(const std::u16string& input_text,
                                           const GURL& destination_url) {
  base::Value::Dict ai_metadata;
  ai_metadata.Set("input", base::UTF16ToUTF8(input_text));
  ai_metadata.Set("destination_url", destination_url.spec());
  chrome::ShowFeedbackPage(
      browser_, feedback::kFeedbackSourceAI,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/
      l10n_util::GetStringUTF8(IDS_HISTORY_EMBEDDINGS_FEEDBACK_PLACEHOLDER),
      /*category_tag=*/"genai_history",
      /*extra_diagnostics=*/std::string(),
      /*autofill_metadata=*/base::Value::Dict(), std::move(ai_metadata));
}

void ChromeOmniboxClient::OnAutocompleteAccept(
    const GURL& destination_url,
    TemplateURLRef::PostContent* post_content,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type match_type,
    base::TimeTicks match_selection_timestamp,
    bool destination_url_entered_without_scheme,
    bool destination_url_entered_with_http_scheme,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match,
    IDNA2008DeviationCharacter deviation_char_in_hostname) {
  TRACE_EVENT("omnibox", "ChromeOmniboxClient::OnAutocompleteAccept", "text",
              text, "match", match, "alternative_nav_match",
              alternative_nav_match);

  // Store the details necessary to open the omnibox match via browser commands.
  location_bar_->set_navigation_params(LocationBar::NavigationParams(
      destination_url, disposition, transition, match_selection_timestamp,
      destination_url_entered_without_scheme,
      destination_url_entered_with_http_scheme, match.extra_headers));

  if (browser_) {
    auto navigation = chrome::OpenCurrentURL(browser_);
    ChromeOmniboxNavigationObserver::Create(navigation.get(), profile_, text,
                                            match, alternative_nav_match);

    // If this navigation was typed by the user and the hostname contained an
    // IDNA 2008 deviation character, record a UKM. See idn_spoof_checker.h
    // for details about deviation characters.
    if (deviation_char_in_hostname != IDNA2008DeviationCharacter::kNone) {
      ukm::SourceId source_id = ukm::ConvertToSourceId(
          navigation->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
      ukm::builders::Navigation_IDNA2008Transition(source_id)
          .SetCharacter(static_cast<int>(deviation_char_in_hostname))
          .Record(ukm::UkmRecorder::Get());
    }
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::MaybeShowExtensionControlledSearchNotification(
      location_bar_->GetWebContents(), match_type);
#endif
}

void ChromeOmniboxClient::OnInputInProgress(bool in_progress) {
  location_bar_->UpdateWithoutTabRestore();
  content::WebContents* const web_contents = location_bar_->GetWebContents();
  if (web_contents) {
    auto* const helper =
        OmniboxTabHelper::FromWebContents(location_bar_->GetWebContents());
    CHECK(helper);
    helper->OnInputInProgress(in_progress);
  }
}

void ChromeOmniboxClient::OnPopupVisibilityChanged(bool popup_is_open) {
  location_bar_->OnPopupVisibilityChanged();
  content::WebContents* const web_contents = location_bar_->GetWebContents();
  if (web_contents) {
    auto* const helper =
        OmniboxTabHelper::FromWebContents(location_bar_->GetWebContents());
    CHECK(helper);
    helper->OnPopupVisibilityChanged(popup_is_open);
  }
}

void ChromeOmniboxClient::OpenIphLink(GURL gurl) {
  ui::PageTransition transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  NavigateParams params(profile_, gurl, transition);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

bool ChromeOmniboxClient::IsHistoryEmbeddingsEnabled() const {
  return history_embeddings::IsHistoryEmbeddingsEnabledForProfile(profile_);
}

base::WeakPtr<OmniboxClient> ChromeOmniboxClient::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ChromeOmniboxClient::DoPrerender(const AutocompleteMatch& match) {
  content::WebContents* web_contents = location_bar_->GetWebContents();

  // Don't prerender when DevTools is open in this tab.
  if (content::DevToolsAgentHost::IsDebuggerAttached(web_contents))
    return;

  // TODO(crbug.com/40208255): Refactor relevant code to reuse common
  // code, and ensure metrics are correctly recorded.
  DCHECK(!AutocompleteMatch::IsSearchType(match.type));
  gfx::Rect container_bounds = web_contents->GetContainerBounds();
  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_)
      ->StartPrerendering(match.destination_url, *web_contents,
                          container_bounds.size());
}

void ChromeOmniboxClient::DoPreconnect(const AutocompleteMatch& match) {
  if (match.destination_url.SchemeIs(extensions::kExtensionScheme))
    return;

  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile_);
  if (loading_predictor) {
    loading_predictor->PrepareForPageLoad(
        /*initiator_origin=*/std::nullopt, match.destination_url,
        predictors::HintOrigin::OMNIBOX,
        predictors::AutocompleteActionPredictor::IsPreconnectable(match));
  }
  // We could prefetch the alternate nav URL, if any, but because there
  // can be many of these as a user types an initial series of characters,
  // the OS DNS cache could suffer eviction problems for minimal gain.
}

void ChromeOmniboxClient::OnBitmapFetched(const BitmapFetchedCallback& callback,
                                          int result_index,
                                          const SkBitmap& bitmap) {
  callback.Run(result_index, bitmap);
}

// static
void ChromeOmniboxClient::OnSuccessfulNavigation(
    Profile* profile,
    const std::u16string& text,
    const AutocompleteMatch& match) {
  auto shortcuts_backend = ShortcutsBackendFactory::GetForProfile(profile);
  // Can be null in incognito.
  if (!shortcuts_backend)
    return;

  shortcuts_backend->AddOrUpdateShortcut(text, match);
}
