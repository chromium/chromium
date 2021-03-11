// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/realbox/realbox_handler.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/search/omnibox_mojo_utils.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/omnibox_controller_emitter.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "net/cookies/cookie_util.h"
#include "ui/base/webui/web_ui_util.h"

RealboxHandler::RealboxHandler(
    mojo::PendingReceiver<realbox::mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents),
      bitmap_fetcher_service_(
          BitmapFetcherServiceFactory::GetForBrowserContext(profile)),
      favicon_cache_(FaviconServiceFactory::GetForProfile(
                         profile,
                         ServiceAccessType::EXPLICIT_ACCESS),
                     HistoryServiceFactory::GetForProfile(
                         profile,
                         ServiceAccessType::EXPLICIT_ACCESS)),
      page_handler_(this, std::move(pending_page_handler)) {}

RealboxHandler::~RealboxHandler() {
  // Clear pending bitmap requests.
  for (auto bitmap_request_id : bitmap_request_ids_) {
    bitmap_fetcher_service_->CancelRequest(bitmap_request_id);
  }
}

void RealboxHandler::SetPage(
    mojo::PendingRemote<realbox::mojom::Page> pending_page) {
  page_.Bind(std::move(pending_page));
}

void RealboxHandler::QueryAutocomplete(const std::u16string& input,
                                       bool prevent_inline_autocomplete) {
  if (!autocomplete_controller_) {
    autocomplete_controller_ = std::make_unique<AutocompleteController>(
        std::make_unique<ChromeAutocompleteProviderClient>(profile_),
        AutocompleteClassifier::DefaultOmniboxProviders());
    autocomplete_controller_->AddObserver(this);

    OmniboxControllerEmitter* emitter =
        OmniboxControllerEmitter::GetForBrowserContext(profile_);
    if (emitter)
      autocomplete_controller_->AddObserver(emitter);
  }

  // TODO(tommycli): We use the input being empty as a signal we are requesting
  // on-focus suggestions. It would be nice if we had a more explicit signal.
  bool is_on_focus = input.empty();

  // Early exit if a query is already in progress for on focus inputs.
  if (!autocomplete_controller_->done() && is_on_focus)
    return;

  if (time_user_first_modified_realbox_.is_null() && !is_on_focus)
    time_user_first_modified_realbox_ = base::TimeTicks::Now();

  AutocompleteInput autocomplete_input(
      input, metrics::OmniboxEventProto::NTP_REALBOX,
      ChromeAutocompleteSchemeClassifier(profile_));
  autocomplete_input.set_focus_type(is_on_focus ? OmniboxFocusType::ON_FOCUS
                                                : OmniboxFocusType::DEFAULT);
  autocomplete_input.set_prevent_inline_autocomplete(
      prevent_inline_autocomplete);

  // We do not want keyword matches for the NTP realbox, which has no UI
  // facilities to support them.
  autocomplete_input.set_prefer_keyword(false);
  autocomplete_input.set_allow_exact_keyword_match(false);

  autocomplete_controller_->Start(autocomplete_input);
}

void RealboxHandler::StopAutocomplete(bool clear_result) {
  if (!autocomplete_controller_)
    return;

  autocomplete_controller_->Stop(clear_result);

  if (clear_result)
    time_user_first_modified_realbox_ = base::TimeTicks();
}

void RealboxHandler::OpenAutocompleteMatch(
    uint8_t line,
    const GURL& url,
    bool are_matches_showing,
    base::TimeDelta time_elapsed_since_last_focus,
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {
  if (autocomplete_controller_->result().size() <= line) {
    return;
  }

  AutocompleteMatch match(autocomplete_controller_->result().match_at(line));
  if (match.destination_url != url) {
    // TODO(https://crbug.com/1020025): this could be malice or staleness.
    // Either way: don't navigate.
    return;
  }

  // TODO(crbug.com/1041129): The following logic for recording Omnibox metrics
  // is largely copied from SearchTabHelper::OpenAutocompleteMatch(). Make sure
  // any changes here is reflected there until one code path is obsolete.

  const auto now = base::TimeTicks::Now();
  base::TimeDelta elapsed_time_since_first_autocomplete_query =
      now - time_user_first_modified_realbox_;
  autocomplete_controller_->UpdateMatchDestinationURLWithQueryFormulationTime(
      elapsed_time_since_first_autocomplete_query, &match);

  LOCAL_HISTOGRAM_BOOLEAN("Omnibox.EventCount", true);

  UMA_HISTOGRAM_MEDIUM_TIMES("Omnibox.FocusToOpenTimeAnyPopupState3",
                             time_elapsed_since_last_focus);

  if (ui::PageTransitionTypeIncludingQualifiersIs(match.transition,
                                                  ui::PAGE_TRANSITION_TYPED)) {
    navigation_metrics::RecordOmniboxURLNavigation(match.destination_url);
  }
  // The following histogram should be recorded for both TYPED and pasted
  // URLs, but should still exclude reloads.
  if (ui::PageTransitionTypeIncludingQualifiersIs(match.transition,
                                                  ui::PAGE_TRANSITION_TYPED) ||
      ui::PageTransitionTypeIncludingQualifiersIs(match.transition,
                                                  ui::PAGE_TRANSITION_LINK)) {
    net::cookie_util::RecordCookiePortOmniboxHistograms(match.destination_url);
  }

  SuggestionAnswer::LogAnswerUsed(match.answer);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (template_url_service &&
      template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
          match.destination_url)) {
    // Note: will always be false for the realbox.
    UMA_HISTOGRAM_BOOLEAN("Omnibox.Search.OffTheRecord",
                          profile_->IsOffTheRecord());
    base::RecordAction(
        base::UserMetricsAction("OmniboxDestinationURLIsSearchOnDSP"));
  }

  AutocompleteMatch::LogSearchEngineUsed(match, template_url_service);

  auto* bookmark_model = BookmarkModelFactory::GetForBrowserContext(profile_);
  if (bookmark_model->IsBookmarked(match.destination_url)) {
    RecordBookmarkLaunch(BOOKMARK_LAUNCH_LOCATION_OMNIBOX,
                         ProfileMetrics::GetBrowserProfileType(profile_));
  }

  const AutocompleteInput& input = autocomplete_controller_->input();
  WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);

  base::TimeDelta default_time_delta = base::TimeDelta::FromMilliseconds(-1);

  if (time_user_first_modified_realbox_.is_null())
    elapsed_time_since_first_autocomplete_query = default_time_delta;

  base::TimeDelta elapsed_time_since_last_change_to_default_match =
      !autocomplete_controller_->last_time_default_match_changed().is_null()
          ? now - autocomplete_controller_->last_time_default_match_changed()
          : default_time_delta;

  OmniboxLog log(
      /*text=*/input.focus_type() != OmniboxFocusType::DEFAULT
          ? std::u16string()
          : input.text(),
      /*just_deleted_text=*/input.prevent_inline_autocomplete(),
      /*input_type=*/input.type(),
      /*in_keyword_mode=*/false,
      /*entry_method=*/metrics::OmniboxEventProto::INVALID,
      /*is_popup_open=*/are_matches_showing,
      /*selected_index=*/line,
      /*disposition=*/disposition,
      /*is_paste_and_go=*/false,
      /*tab_id=*/sessions::SessionTabHelper::IdForTab(web_contents_),
      /*current_page_classification=*/metrics::OmniboxEventProto::NTP_REALBOX,
      /*elapsed_time_since_user_first_modified_omnibox=*/
      elapsed_time_since_first_autocomplete_query,
      /*completed_length=*/match.allowed_to_be_default_match
          ? match.inline_autocompletion.length()
          : std::u16string::npos,
      /*elapsed_time_since_last_change_to_default_match=*/
      elapsed_time_since_last_change_to_default_match,
      /*result=*/autocomplete_controller_->result());
  autocomplete_controller_->AddProviderAndTriggeringLogs(&log);

  OmniboxEventGlobalTracker::GetInstance()->OnURLOpened(&log);

  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_)
      ->OnOmniboxOpenedUrl(log);

  web_contents_->OpenURL(
      content::OpenURLParams(match.destination_url, content::Referrer(),
                             disposition, match.transition, false));
}

void RealboxHandler::DeleteAutocompleteMatch(uint8_t line) {
  if (autocomplete_controller_->result().size() <= line ||
      !autocomplete_controller_->result().match_at(line).SupportsDeletion()) {
    return;
  }

  const auto& match = autocomplete_controller_->result().match_at(line);
  if (match.SupportsDeletion()) {
    autocomplete_controller_->Stop(false);
    autocomplete_controller_->DeleteMatch(match);
  }
}

void RealboxHandler::ToggleSuggestionGroupIdVisibility(
    int32_t suggestion_group_id) {
  if (!autocomplete_controller_)
    return;

  omnibox::SuggestionGroupVisibility new_value =
      autocomplete_controller_->result().IsSuggestionGroupIdHidden(
          profile_->GetPrefs(), suggestion_group_id)
          ? omnibox::SuggestionGroupVisibility::SHOWN
          : omnibox::SuggestionGroupVisibility::HIDDEN;
  omnibox::SetSuggestionGroupVisibility(profile_->GetPrefs(),
                                        suggestion_group_id, new_value);
}

void RealboxHandler::LogCharTypedToRepaintLatency(base::TimeDelta latency) {
  UMA_HISTOGRAM_TIMES("NewTabPage.Realbox.CharTypedToRepaintLatency.ToPaint",
                      latency);
}

void RealboxHandler::OnResultChanged(AutocompleteController* controller,
                                     bool default_match_changed) {
  DCHECK(controller == autocomplete_controller_.get());

  page_->AutocompleteResultChanged(omnibox::CreateAutocompleteResult(
      autocomplete_controller_->input().text(),
      autocomplete_controller_->result(),
      BookmarkModelFactory::GetForBrowserContext(profile_),
      profile_->GetPrefs()));

  // Clear pending bitmap requests before requesting new ones.
  for (auto bitmap_request_id : bitmap_request_ids_) {
    bitmap_fetcher_service_->CancelRequest(bitmap_request_id);
  }
  bitmap_request_ids_.clear();

  int match_index = -1;
  for (const auto& match : autocomplete_controller_->result()) {
    match_index++;

    // Request bitmaps for matche images.
    if (!match.image_url.is_empty()) {
      bitmap_request_ids_.push_back(bitmap_fetcher_service_->RequestImage(
          match.image_url,
          base::BindOnce(&RealboxHandler::OnRealboxBitmapFetched,
                         weak_ptr_factory_.GetWeakPtr(), match_index,
                         match.image_url)));
    }

    // Request favicons for navigational matches.
    // TODO(crbug.com/1075848): Investigate using chrome://favicon2.
    if (!AutocompleteMatch::IsSearchType(match.type) &&
        match.type != AutocompleteMatchType::DOCUMENT_SUGGESTION) {
      gfx::Image favicon = favicon_cache_.GetLargestFaviconForPageUrl(
          match.destination_url,
          base::BindOnce(&RealboxHandler::OnRealboxFaviconFetched,
                         weak_ptr_factory_.GetWeakPtr(), match_index,
                         match.destination_url));
      if (!favicon.IsEmpty()) {
        OnRealboxFaviconFetched(match_index, match.destination_url, favicon);
      }
    }
  }
}

void RealboxHandler::OnRealboxBitmapFetched(int match_index,
                                            const GURL& image_url,
                                            const SkBitmap& bitmap) {
  auto data = gfx::Image::CreateFrom1xBitmap(bitmap).As1xPNGBytes();
  std::string data_url =
      webui::GetPngDataUrl(data->front_as<unsigned char>(), data->size());

  page_->AutocompleteMatchImageAvailable(match_index, image_url, data_url);
}

void RealboxHandler::OnRealboxFaviconFetched(int match_index,
                                             const GURL& page_url,
                                             const gfx::Image& favicon) {
  DCHECK(!favicon.IsEmpty());
  auto data = favicon.As1xPNGBytes();
  std::string data_url =
      webui::GetPngDataUrl(data->front_as<unsigned char>(), data->size());

  page_->AutocompleteMatchImageAvailable(match_index, page_url, data_url);
}
