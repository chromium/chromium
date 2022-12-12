// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/time/time_to_iso8601.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_metrics_logger.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/image_service/image_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/cluster_metrics_utils.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_clusters/ui/query_clusters_state.h"
#include "components/image_service/image_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom.h"
#include "url/gurl.h"

namespace history_clusters {

namespace {

class HistoryClustersSidePanelContextMenu
    : public ui::SimpleMenuModel,
      public ui::SimpleMenuModel::Delegate {
 public:
  HistoryClustersSidePanelContextMenu(Browser* browser, GURL url)
      : ui::SimpleMenuModel(this), browser_(browser), url_(url) {
    AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
                        IDS_HISTORY_CLUSTERS_OPEN_IN_NEW_TAB);
    AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW,
                        IDS_HISTORY_CLUSTERS_OPEN_IN_NEW_WINDOW);
    AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD,
                        IDS_HISTORY_CLUSTERS_OPEN_INCOGNITO);
    AddSeparator(ui::NORMAL_SEPARATOR);

    AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYLINKLOCATION,
                        IDS_HISTORY_CLUSTERS_COPY_LINK);
  }
  ~HistoryClustersSidePanelContextMenu() override = default;

  void ExecuteCommand(int command_id, int event_flags) override {
    switch (command_id) {
      case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB: {
        content::OpenURLParams params(url_, content::Referrer(),
                                      WindowOpenDisposition::NEW_BACKGROUND_TAB,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
        browser_->OpenURL(params);
        break;
      }

      case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW: {
        content::OpenURLParams params(url_, content::Referrer(),
                                      WindowOpenDisposition::NEW_WINDOW,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
        browser_->OpenURL(params);
        break;
      }

      case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD: {
        content::OpenURLParams params(url_, content::Referrer(),
                                      WindowOpenDisposition::OFF_THE_RECORD,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
        browser_->OpenURL(params);
        break;
      }
      case IDC_CONTENT_CONTEXT_COPYLINKLOCATION: {
        ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
        scw.WriteText(base::UTF8ToUTF16(url_.spec()));
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }

 private:
  const raw_ptr<Browser> browser_;
  GURL url_;
};

// Creates a `mojom::VisitPtr` from a `history_clusters::Visit`.
mojom::URLVisitPtr VisitToMojom(Profile* profile,
                                const history::ClusterVisit& visit) {
  auto visit_mojom = mojom::URLVisit::New();
  visit_mojom->normalized_url = visit.normalized_url;
  visit_mojom->url_for_display = base::UTF16ToUTF8(visit.url_for_display);
  if (!visit.image_url.is_empty()) {
    visit_mojom->image_url = visit.image_url;
  }

  // Add the raw URLs and visit times so the UI can perform deletion.
  auto& annotated_visit = visit.annotated_visit;
  visit_mojom->raw_visit_data = mojom::RawVisitData::New(
      annotated_visit.url_row.url(), annotated_visit.visit_row.visit_time);
  for (const auto& duplicate : visit.duplicate_visits) {
    visit_mojom->duplicates.push_back(
        mojom::RawVisitData::New(duplicate.url, duplicate.visit_time));
  }

  visit_mojom->page_title = base::UTF16ToUTF8(annotated_visit.url_row.title());

  for (const auto& match : visit.title_match_positions) {
    auto match_mojom = mojom::MatchPosition::New();
    match_mojom->begin = match.first;
    match_mojom->end = match.second;
    visit_mojom->title_match_positions.push_back(std::move(match_mojom));
  }
  for (const auto& match : visit.url_for_display_match_positions) {
    auto match_mojom = mojom::MatchPosition::New();
    match_mojom->begin = match.first;
    match_mojom->end = match.second;
    visit_mojom->url_for_display_match_positions.push_back(
        std::move(match_mojom));
  }

  visit_mojom->relative_date = base::UTF16ToUTF8(ui::TimeFormat::Simple(
      ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
      base::Time::Now() - annotated_visit.visit_row.visit_time));
  if (annotated_visit.context_annotations.is_existing_part_of_tab_group ||
      annotated_visit.context_annotations.is_placed_in_tab_group) {
    visit_mojom->annotations.push_back(mojom::Annotation::kTabGrouped);
  }
  if (annotated_visit.context_annotations.is_existing_bookmark ||
      annotated_visit.context_annotations.is_new_bookmark) {
    visit_mojom->annotations.push_back(mojom::Annotation::kBookmarked);
  }

  const TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  const TemplateURL* default_search_provider =
      template_url_service ? template_url_service->GetDefaultSearchProvider()
                           : nullptr;
  if (default_search_provider &&
      default_search_provider->IsSearchURL(
          visit.normalized_url, template_url_service->search_terms_data())) {
    visit_mojom->annotations.push_back(mojom::Annotation::kSearchResultsPage);
  }

  visit_mojom->hidden = visit.hidden;

  if (GetConfig().user_visible_debug) {
    visit_mojom->debug_info["visit_id"] =
        base::NumberToString(annotated_visit.visit_row.visit_id);
    visit_mojom->debug_info["score"] = base::NumberToString(visit.score);
    visit_mojom->debug_info["visit_time"] =
        base::TimeToISO8601(visit.annotated_visit.visit_row.visit_time);
    visit_mojom->debug_info["foreground_duration"] =
        base::NumberToString(annotated_visit.context_annotations
                                 .total_foreground_duration.InSecondsF());
    visit_mojom->debug_info["visit_source"] =
        base::NumberToString(annotated_visit.source);
  }

  return visit_mojom;
}

// Creates a `mojom::SearchQueryPtr` from the given search query, if possible.
absl::optional<mojom::SearchQueryPtr> SearchQueryToMojom(
    Profile* profile,
    const std::string& search_query) {
  const TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  const TemplateURL* default_search_provider =
      template_url_service ? template_url_service->GetDefaultSearchProvider()
                           : nullptr;
  if (!default_search_provider) {
    return absl::nullopt;
  }

  const std::string url = default_search_provider->url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(base::UTF8ToUTF16(search_query)),
      template_url_service->search_terms_data());
  if (url.empty()) {
    return absl::nullopt;
  }

  auto search_query_mojom = mojom::SearchQuery::New();
  search_query_mojom->query = search_query;
  search_query_mojom->url = GURL(url);
  return search_query_mojom;
}

void ShowSurveyAndLogMetrics(HatsService* service,
                             content::WebContents* contents,
                             const std::string& trigger,
                             base::TimeDelta delay) {
  DCHECK(service);
  DCHECK(contents);

  base::UmaHistogramBoolean("History.Clusters.Survey.CanShowAnySurvey",
                            service->CanShowAnySurvey(/*user_prompted=*/false));
  base::UmaHistogramBoolean("History.Clusters.Survey.CanShowSurvey",
                            service->CanShowSurvey(trigger));

  bool success = service->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerJourneysHistoryEntrypoint, contents,
      delay.InMilliseconds());
  base::UmaHistogramBoolean("History.Clusters.Survey.Success", success);
}

}  // namespace

// Creates a `mojom::QueryResultPtr` using the original `query`, if the query
// was a continuation one, and the result of querying HistoryClustersService.
mojom::QueryResultPtr QueryClustersResultToMojom(
    Profile* profile,
    const std::string& query,
    const std::vector<history::Cluster> clusters_batch,
    bool can_load_more,
    bool is_continuation) {
  std::vector<mojom::ClusterPtr> cluster_mojoms;
  for (const auto& cluster : clusters_batch) {
    auto cluster_mojom = mojom::Cluster::New();
    cluster_mojom->id = cluster.cluster_id;
    if (cluster.label) {
      cluster_mojom->label = base::UTF16ToUTF8(*cluster.label);
      for (const auto& match : cluster.label_match_positions) {
        auto match_mojom = mojom::MatchPosition::New();
        match_mojom->begin = match.first;
        match_mojom->end = match.second;
        cluster_mojom->label_match_positions.push_back(std::move(match_mojom));
      }
    }

    if (GetConfig().user_visible_debug && cluster.from_persistence) {
      cluster_mojom->debug_info =
          "persisted, id = " + base::NumberToString(cluster.cluster_id);
    }

    for (const auto& visit : cluster.visits) {
      cluster_mojom->visits.push_back(VisitToMojom(profile, visit));
    }

    for (const auto& related_search : cluster.related_searches) {
      auto search_query_mojom = SearchQueryToMojom(profile, related_search);
      if (search_query_mojom) {
        cluster_mojom->related_searches.emplace_back(
            std::move(*search_query_mojom));
      }
    }

    cluster_mojoms.emplace_back(std::move(cluster_mojom));
  }

  auto result_mojom = mojom::QueryResult::New();
  result_mojom->query = query;
  result_mojom->clusters = std::move(cluster_mojoms);
  result_mojom->can_load_more = can_load_more;
  result_mojom->is_continuation = is_continuation;
  return result_mojom;
}

HistoryClustersHandler::HistoryClustersHandler(
    mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents),
      page_handler_(this, std::move(pending_page_handler)) {
  DCHECK(profile_);
  DCHECK(web_contents_);

  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(profile_);
  DCHECK(history_clusters_service);
  service_observation_.Observe(history_clusters_service);

  history::HistoryService* local_history = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  browsing_history_service_ = std::make_unique<history::BrowsingHistoryService>(
      this, local_history, sync_service);
}

HistoryClustersHandler::~HistoryClustersHandler() = default;

void HistoryClustersHandler::SetSidePanelUIEmbedder(
    base::WeakPtr<ui::MojoBubbleWebUIController::Embedder>
        side_panel_embedder) {
  history_clusters_side_panel_embedder_ = side_panel_embedder;
}

void HistoryClustersHandler::SetQuery(const std::string& query) {
  if (page_) {
    page_->OnQueryChangedByUser(query);
  }
}

void HistoryClustersHandler::OpenHistoryCluster(
    const GURL& url,
    ui::mojom::ClickModifiersPtr click_modifiers) {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;

  // In the Side Panel, the default is the current tab. From History WebUI, the
  // default is a new foreground tab.
  WindowOpenDisposition default_disposition =
      history_clusters_side_panel_embedder_
          ? WindowOpenDisposition::CURRENT_TAB
          : WindowOpenDisposition::NEW_FOREGROUND_TAB;

  WindowOpenDisposition open_location = ui::DispositionFromClick(
      click_modifiers->middle_button, click_modifiers->alt_key,
      click_modifiers->ctrl_key, click_modifiers->meta_key,
      click_modifiers->shift_key, default_disposition);
  content::OpenURLParams params(url, content::Referrer(), open_location,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                /*is_renderer_initiated=*/false);
  browser->OpenURL(params);
}

void HistoryClustersHandler::SetPage(
    mojo::PendingRemote<mojom::Page> pending_page) {
  page_.Bind(std::move(pending_page));
}

void HistoryClustersHandler::ShowSidePanelUI() {
  if (history_clusters_side_panel_embedder_) {
    history_clusters_side_panel_embedder_->ShowUI();
  }
}

void HistoryClustersHandler::ToggleVisibility(
    bool visible,
    ToggleVisibilityCallback callback) {
  profile_->GetPrefs()->SetBoolean(prefs::kVisible, visible);
  std::move(callback).Run(visible);
}

void HistoryClustersHandler::StartQueryClusters(const std::string& query,
                                                bool recluster) {
  last_query_issued_ = query;

  if (!query.empty()) {
    // If the query string is not empty, we assume that this clusters query
    // is user generated.
    HistoryClustersMetricsLogger::GetOrCreateForPage(
        web_contents_->GetPrimaryPage())
        ->increment_query_count();
  }

  // Since the query has changed, initialize a new QueryClustersState and
  // request the first batch of clusters.
  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(profile_);
  auto* image_service =
      image_service::ImageServiceFactory::GetForBrowserContext(profile_);
  query_clusters_state_ = std::make_unique<QueryClustersState>(
      history_clusters_service->GetWeakPtr(), image_service->GetWeakPtr(),
      query, recluster);
  LoadMoreClusters(query);
}

void HistoryClustersHandler::LoadMoreClusters(const std::string& query) {
  if (query_clusters_state_) {
    DCHECK_EQ(query, query_clusters_state_->query());
    query_clusters_state_->LoadNextBatchOfClusters(
        base::BindOnce(&HistoryClustersHandler::SendClustersToPage,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void HistoryClustersHandler::RemoveVisits(
    std::vector<mojom::URLVisitPtr> visits,
    RemoveVisitsCallback callback) {
  if (!profile_->GetPrefs()->GetBoolean(
          ::prefs::kAllowDeletingBrowserHistory) ||
      visits.empty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // If there's a pending request for deletion, we have to fail here, because
  // `BrowsingHistoryService` only supports one deletion request at a time.
  if (!pending_remove_visits_callback_.is_null()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::vector<history::BrowsingHistoryService::HistoryEntry> items_to_remove;
  for (const auto& visit : visits) {
    {
      history::BrowsingHistoryService::HistoryEntry entry;
      entry.url = visit->raw_visit_data->url;
      entry.all_timestamps.insert(
          visit->raw_visit_data->visit_time.ToInternalValue());
      items_to_remove.push_back(std::move(entry));
    }
    for (const auto& duplicate : visit->duplicates) {
      history::BrowsingHistoryService::HistoryEntry entry;
      entry.url = duplicate->url;
      entry.all_timestamps.insert(duplicate->visit_time.ToInternalValue());
      items_to_remove.push_back(std::move(entry));
    }
  }

  // Transfer the visits pending deletion and the respective callback to member
  // variables.
  pending_remove_visits_ = std::move(visits);
  pending_remove_visits_callback_ = std::move(callback);

  browsing_history_service_->RemoveVisits(items_to_remove);
}

void HistoryClustersHandler::OpenVisitUrlsInTabGroup(
    std::vector<mojom::URLVisitPtr> visits) {
  auto* browser = chrome::FindTabbedBrowser(profile_, false);
  if (!browser) {
    return;
  }

  // Hard cap the number of opened visits in a tab group to 32. It's a
  // relatively high cap chosen fairly arbitrarily, because the user took an
  // affirmative action to open this many tabs. And hidden visits aren't opened.
  constexpr size_t kMaxVisitsToOpenInTabGroup = 32;
  if (visits.size() > kMaxVisitsToOpenInTabGroup) {
    visits.resize(kMaxVisitsToOpenInTabGroup);
  }

  auto* model = browser->tab_strip_model();
  std::vector<int> tab_indices;
  tab_indices.reserve(visits.size());
  for (const auto& visit_ptr : visits) {
    auto* opened_web_contents = browser->OpenURL(
        content::OpenURLParams(visit_ptr->normalized_url, content::Referrer(),
                               WindowOpenDisposition::NEW_BACKGROUND_TAB,
                               ui::PAGE_TRANSITION_AUTO_BOOKMARK, false));

    // Only add those tabs to a new group that actually opened in this browser.
    const int tab_index = model->GetIndexOfWebContents(opened_web_contents);
    if (tab_index != TabStripModel::kNoTab) {
      tab_indices.push_back(tab_index);
    }
  }
  // Sometimes tab_indices is empty, and TabStripModel::AddToNewGroup
  // requires a non-empty vector (Fixes https://crbug.com/1339140)
  if (tab_indices.empty()) {
    return;
  }
  model->AddToNewGroup(tab_indices);
}

void HistoryClustersHandler::OnDebugMessage(const std::string& message) {
  content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
  if (rfh && GetConfig().non_user_visible_debug) {
    rfh->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kInfo, message);
  }
}

void HistoryClustersHandler::OnRemoveVisitsComplete() {
  DCHECK(!pending_remove_visits_callback_.is_null());
  std::move(pending_remove_visits_callback_).Run(/*success=*/true);
  // Notify the page of the successfully deleted visits to update the UI.
  page_->OnVisitsRemoved(std::move(pending_remove_visits_));
}

void HistoryClustersHandler::OnRemoveVisitsFailed() {
  DCHECK(!pending_remove_visits_callback_.is_null());
  std::move(pending_remove_visits_callback_).Run(/*success=*/false);
}

void HistoryClustersHandler::HistoryDeleted() {
  if (page_) {
    page_->OnHistoryDeleted();
  }
}

Profile* HistoryClustersHandler::GetProfile() {
  DCHECK(profile_);
  return profile_;
}

void HistoryClustersHandler::SendClustersToPage(
    const std::string& query,
    const std::vector<history::Cluster> clusters_batch,
    bool can_load_more,
    bool is_continuation) {
  auto query_result =
      QueryClustersResultToMojom(profile_, query, std::move(clusters_batch),
                                 can_load_more, is_continuation);
  page_->OnClustersQueryResult(std::move(query_result));

  // The user loading their first set of clusters should start the timer for
  // launching the Journeys survey.
  LaunchJourneysSurvey();
}

void HistoryClustersHandler::LaunchJourneysSurvey() {
  // All the below is to attempt launch a survey, after loading the first set of
  // clusters.
  if (survey_launch_attempted_) {
    return;
  }
  survey_launch_attempted_ = true;

  HatsService* hats_service = HatsServiceFactory::GetForProfile(profile_, true);
  if (!hats_service) {
    return;
  }

  auto* logger =
      history_clusters::HistoryClustersMetricsLogger::GetOrCreateForPage(
          web_contents_->GetPrimaryPage());
  auto initial_state = logger->initial_state();
  if (!initial_state) {
    return;
  }

  constexpr char kHistoryClustersSurveyRequestedUmaName[] =
      "History.Clusters.Survey.Requested";
  // These values must match enums.xml, and should not be modified.
  enum HistoryClustersSurvey {
    kHistoryEntrypoint = 0,
    kOmniboxEntrypoint = 1,
    kMaxValue = kOmniboxEntrypoint,
  };
  if (*initial_state ==
          history_clusters::HistoryClustersInitialState::kSameDocument &&
      base::FeatureList::IsEnabled(kJourneysSurveyForHistoryEntrypoint)) {
    // Same document navigation basically means clicking over from History.
    ShowSurveyAndLogMetrics(hats_service, web_contents_,
                            kHatsSurveyTriggerJourneysHistoryEntrypoint,
                            kJourneysSurveyForHistoryEntrypointDelay.Get());
    base::UmaHistogramEnumeration(kHistoryClustersSurveyRequestedUmaName,
                                  kHistoryEntrypoint);
  } else if (*initial_state == history_clusters::HistoryClustersInitialState::
                                   kIndirectNavigation &&
             base::FeatureList::IsEnabled(
                 kJourneysSurveyForOmniboxEntrypoint)) {
    // Indirect navigation basically means from the omnibox.
    ShowSurveyAndLogMetrics(hats_service, web_contents_,
                            kHatsSurveyTriggerJourneysOmniboxEntrypoint,
                            kJourneysSurveyForOmniboxEntrypointDelay.Get());
    base::UmaHistogramEnumeration(kHistoryClustersSurveyRequestedUmaName,
                                  kOmniboxEntrypoint);
  }
}

void HistoryClustersHandler::RecordVisitAction(mojom::VisitAction visit_action,
                                               uint32_t visit_index,
                                               mojom::VisitType visit_type) {
  HistoryClustersMetricsLogger::GetOrCreateForPage(
      web_contents_->GetPrimaryPage())
      ->RecordVisitAction(static_cast<VisitAction>(visit_action), visit_index,
                          static_cast<VisitType>(visit_type));
}

void HistoryClustersHandler::RecordClusterAction(
    mojom::ClusterAction cluster_action,
    uint32_t cluster_index) {
  HistoryClustersMetricsLogger::GetOrCreateForPage(
      web_contents_->GetPrimaryPage())
      ->RecordClusterAction(static_cast<ClusterAction>(cluster_action),
                            cluster_index);
}

void HistoryClustersHandler::RecordRelatedSearchAction(
    mojom::RelatedSearchAction action,
    uint32_t related_search_index) {
  HistoryClustersMetricsLogger::GetOrCreateForPage(
      web_contents_->GetPrimaryPage())
      ->RecordRelatedSearchAction(static_cast<RelatedSearchAction>(action),
                                  related_search_index);
}

void HistoryClustersHandler::RecordToggledVisibility(bool visible) {
  HistoryClustersMetricsLogger::GetOrCreateForPage(
      web_contents_->GetPrimaryPage())
      ->RecordToggledVisibility(visible);
}

void HistoryClustersHandler::ShowContextMenuForURL(const GURL& url,
                                                   const gfx::Point& point) {
  Browser* browser = chrome::FindLastActive();
  if (history_clusters_side_panel_embedder_) {
    history_clusters_side_panel_embedder_->ShowContextMenu(
        point,
        std::make_unique<HistoryClustersSidePanelContextMenu>(browser, url));
  }
}

}  // namespace history_clusters
