// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_navigation_observer.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_service.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/omnibox_controller_emitter.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prerender/browser/prerender_field_trial.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

using predictors::AutocompleteActionPredictor;

ChromeOmniboxClient::ChromeOmniboxClient(OmniboxEditController* controller,
                                         Profile* profile)
    : controller_(static_cast<ChromeOmniboxEditController*>(controller)),
      profile_(profile),
      scheme_classifier_(profile),
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

std::unique_ptr<OmniboxNavigationObserver>
ChromeOmniboxClient::CreateOmniboxNavigationObserver(
    const base::string16& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternate_nav_match) {
  return std::make_unique<ChromeOmniboxNavigationObserver>(
      profile_, text, match, alternate_nav_match);
}

bool ChromeOmniboxClient::CurrentPageExists() const {
  return (controller_->GetWebContents() != nullptr);
}

const GURL& ChromeOmniboxClient::GetURL() const {
  return CurrentPageExists() ? controller_->GetWebContents()->GetVisibleURL()
                             : GURL::EmptyGURL();
}

const base::string16& ChromeOmniboxClient::GetTitle() const {
  return CurrentPageExists() ? controller_->GetWebContents()->GetTitle()
                             : base::EmptyString16();
}

gfx::Image ChromeOmniboxClient::GetFavicon() const {
  return favicon::ContentFaviconDriver::FromWebContents(
             controller_->GetWebContents())
      ->GetFavicon();
}

bool ChromeOmniboxClient::IsLoading() const {
  return controller_->GetWebContents()->IsLoading();
}

bool ChromeOmniboxClient::IsPasteAndGoEnabled() const {
  return controller_->command_updater()->IsCommandEnabled(IDC_OPEN_CURRENT_URL);
}

bool ChromeOmniboxClient::IsDefaultSearchProviderEnabled() const {
  const base::DictionaryValue* url_dict = profile_->GetPrefs()->GetDictionary(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
  bool disabled_by_policy = false;
  url_dict->GetBoolean(DefaultSearchManager::kDisabledByPolicy,
                       &disabled_by_policy);
  return !disabled_by_policy;
}

const SessionID& ChromeOmniboxClient::GetSessionID() const {
  return sessions::SessionTabHelper::FromWebContents(
             controller_->GetWebContents())
      ->session_id();
}

bookmarks::BookmarkModel* ChromeOmniboxClient::GetBookmarkModel() {
  return BookmarkModelFactory::GetForBrowserContext(profile_);
}

OmniboxControllerEmitter* ChromeOmniboxClient::GetOmniboxControllerEmitter() {
  return OmniboxControllerEmitter::GetForBrowserContext(profile_);
}

TemplateURLService* ChromeOmniboxClient::GetTemplateURLService() {
  return TemplateURLServiceFactory::GetForProfile(profile_);
}

const AutocompleteSchemeClassifier& ChromeOmniboxClient::GetSchemeClassifier()
    const {
  return scheme_classifier_;
}

AutocompleteClassifier* ChromeOmniboxClient::GetAutocompleteClassifier() {
  return AutocompleteClassifierFactory::GetForProfile(profile_);
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
  gfx::Insets padding_border((icon_size - icon.Height()) / 2,
                             (icon_size - icon.Width()) / 2);
  if (!padding_border.IsEmpty()) {
    return gfx::Image(gfx::CanvasImageSource::CreatePadded(*icon.ToImageSkia(),
                                                           padding_border));
  }
  return icon;
}

bool ChromeOmniboxClient::ProcessExtensionKeyword(
    const TemplateURL* template_url,
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition,
    OmniboxNavigationObserver* observer) {
  if (template_url->type() != TemplateURL::OMNIBOX_API_EXTENSION)
    return false;

  // Strip the keyword + leading space off the input, but don't exceed
  // fill_into_edit.  An obvious case is that the user may not have entered
  // a leading space and is asking to launch this extension without any
  // additional input.
  size_t prefix_length =
      std::min(match.keyword.length() + 1, match.fill_into_edit.length());
  extensions::ExtensionOmniboxEventRouter::OnInputEntered(
      controller_->GetWebContents(),
      template_url->GetExtensionId(),
      base::UTF16ToUTF8(match.fill_into_edit.substr(prefix_length)),
      disposition);

  static_cast<ChromeOmniboxNavigationObserver*>(observer)
      ->OnSuccessfulNavigation();
  return true;
}

void ChromeOmniboxClient::OnInputStateChanged() {
  if (!controller_->GetWebContents())
    return;
  if (auto* helper =
          OmniboxTabHelper::FromWebContents(controller_->GetWebContents())) {
    helper->OnInputStateChanged();
  }
}

void ChromeOmniboxClient::OnFocusChanged(
    OmniboxFocusState state,
    OmniboxFocusChangeReason reason) {
  if (!controller_->GetWebContents())
    return;
  if (auto* helper =
          OmniboxTabHelper::FromWebContents(controller_->GetWebContents())) {
    helper->OnFocusChanged(state, reason);
  }
  WakeupDecoder();
}

void ChromeOmniboxClient::OnResultChanged(
    const AutocompleteResult& result,
    bool default_match_changed,
    const BitmapFetchedCallback& on_bitmap_fetched) {
  auto now = base::TimeTicks::Now();

  BitmapFetcherService* bitmap_fetcher_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(profile_);

  // Clear out the old requests.
  for (auto request_id : request_ids_) {
    bitmap_fetcher_service->CancelRequest(request_id);
  }
  request_ids_.clear();
  // Create new requests.
  int result_index = -1;
  for (const auto& match : result) {
    ++result_index;
    if (match.ImageUrl().is_empty()) {
      continue;
    }

    request_ids_.push_back(bitmap_fetcher_service->RequestImage(
        match.ImageUrl(),
        base::BindOnce(
            &ChromeOmniboxClient::OnBitmapFetched, weak_factory_.GetWeakPtr(),
            on_bitmap_fetched, result_index,
            bitmap_fetcher_service->IsCached(match.ImageUrl()), now)));
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

void ChromeOmniboxClient::OnCurrentMatchChanged(
    const AutocompleteMatch& match) {
  if (!prerender::IsNoStatePrefetchEnabled())
    DoPreconnect(match);
}

void ChromeOmniboxClient::OnTextChanged(const AutocompleteMatch& current_match,
                                        bool user_input_in_progress,
                                        const base::string16& user_text,
                                        const AutocompleteResult& result,
                                        bool has_focus) {
  AutocompleteActionPredictor::Action recommended_action =
      AutocompleteActionPredictor::ACTION_NONE;
  if (user_input_in_progress) {
    AutocompleteActionPredictor* action_predictor =
        predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_);
    action_predictor->RegisterTransitionalMatches(user_text, result);
    // Confer with the AutocompleteActionPredictor to determine what action,
    // if any, we should take. Get the recommended action here even if we
    // don't need it so we can get stats for anyone who is opted in to UMA,
    // but only get it if the user has actually typed something to avoid
    // constructing it before it's needed. Note: This event is triggered as
    // part of startup when the initial tab transitions to the start page.
    recommended_action =
        action_predictor->RecommendAction(user_text, current_match);
  }

  UMA_HISTOGRAM_ENUMERATION("AutocompleteActionPredictor.Action",
                            recommended_action,
                            AutocompleteActionPredictor::LAST_PREDICT_ACTION);

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
  WakeupDecoder();
}

void ChromeOmniboxClient::OnRevert() {
  AutocompleteActionPredictor* action_predictor =
      predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_);
  action_predictor->ClearTransitionalMatches();
  action_predictor->CancelPrerender();
}

void ChromeOmniboxClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_)
      ->OnOmniboxOpenedUrl(*log);
}

void ChromeOmniboxClient::OnBookmarkLaunched() {
  RecordBookmarkLaunch(BOOKMARK_LAUNCH_LOCATION_OMNIBOX,
                       ProfileMetrics::GetBrowserProfileType(profile_));
}

void ChromeOmniboxClient::DiscardNonCommittedNavigations() {
  controller_->GetWebContents()->GetController().DiscardNonCommittedEntries();
}

void ChromeOmniboxClient::NewIncognitoWindow() {
  chrome::NewIncognitoWindow(profile_);
}

void ChromeOmniboxClient::PromptPageTranslation() {
  content::WebContents* contents = controller_->GetWebContents();
  if (contents) {
    ChromeTranslateClient* translate_client =
        ChromeTranslateClient::FromWebContents(contents);
    if (translate_client) {
      DCHECK_NE(nullptr, translate_client->GetTranslateManager());
      translate_client->GetTranslateManager()->InitiateManualTranslation(true);
    }
  }
}

void ChromeOmniboxClient::OpenUpdateChromeDialog() {
  const content::WebContents* contents = controller_->GetWebContents();
  if (contents) {
    Browser* browser = chrome::FindBrowserWithWebContents(contents);
    if (browser) {
      // Here we record and take action more directly than
      // chrome::OpenUpdateChromeDialog because that call is intended for use
      // by the delayed-update/auto-nag system, possibly presenting dialogs
      // that don't apply when the goal is immediate relaunch & update.
      // TODO(orinj): Ensure that this is the correct way to handle
      // explicitly requested update regardless of the kind of update ready.
      // See comments at https://crrev.com/c/1281162 for context.
      base::RecordAction(base::UserMetricsAction("UpdateChrome"));
      browser->window()->ShowUpdateChromeDialog();
    }
  }
}

void ChromeOmniboxClient::DoPrerender(
    const AutocompleteMatch& match) {
  content::WebContents* web_contents = controller_->GetWebContents();

  // Don't prerender when DevTools is open in this tab.
  if (content::DevToolsAgentHost::IsDebuggerAttached(web_contents))
    return;

  gfx::Rect container_bounds = web_contents->GetContainerBounds();

  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_)
      ->StartPrerendering(
          match.destination_url,
          web_contents->GetController().GetDefaultSessionStorageNamespace(),
          container_bounds.size());
}

void ChromeOmniboxClient::DoPreconnect(const AutocompleteMatch& match) {
  if (match.destination_url.SchemeIs(extensions::kExtensionScheme))
    return;

  // Warm up DNS Prefetch cache, or preconnect to a search service.
  UMA_HISTOGRAM_ENUMERATION("Autocomplete.MatchType", match.type,
                            AutocompleteMatchType::NUM_TYPES);
  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile_);
  if (loading_predictor) {
    loading_predictor->PrepareForPageLoad(
        match.destination_url, predictors::HintOrigin::OMNIBOX,
        predictors::AutocompleteActionPredictor::IsPreconnectable(match));
  }
  // We could prefetch the alternate nav URL, if any, but because there
  // can be many of these as a user types an initial series of characters,
  // the OS DNS cache could suffer eviction problems for minimal gain.
}

void ChromeOmniboxClient::WakeupDecoder() {
  if (base::GetFieldTrialParamByFeatureAsBool(
          omnibox::kEntitySuggestionsReduceLatency,
          OmniboxFieldTrial::kEntitySuggestionsReduceLatencyDecoderWakeupParam,
          false)) {
    if (auto* service =
            BitmapFetcherServiceFactory::GetForBrowserContext(profile_)) {
      service->WakeupDecoder();
    }
  }
}

void ChromeOmniboxClient::OnBitmapFetched(const BitmapFetchedCallback& callback,
                                          int result_index,
                                          bool is_cached,
                                          base::TimeTicks start_time,
                                          const SkBitmap& bitmap) {
  auto time_delta = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES("Omnibox.BitmapFetchLatency", time_delta);
  if (is_cached)
    UMA_HISTOGRAM_TIMES("Omnibox.BitmapFetchLatency.Cached", time_delta);
  else
    UMA_HISTOGRAM_TIMES("Omnibox.BitmapFetchLatency.Uncached", time_delta);
  callback.Run(result_index, bitmap);
}
