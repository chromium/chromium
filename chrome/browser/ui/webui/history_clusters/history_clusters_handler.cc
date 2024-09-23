// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_metrics_logger.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/common/pref_names.h"
#include "components/history_clusters/core/cluster_metrics_utils.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_cluster_type_utils.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_clusters/core/query_clusters_state.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/page_image_service/image_service.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/actions/actions.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom.h"
#include "url/gurl.h"

namespace history_clusters {

namespace {

void InvokeAction(actions::ActionId id, actions::ActionItem* scope) {
  actions::ActionManager::Get().FindAction(id, scope)->InvokeAction();
}

// Returns the current browser window, regardless of whether this instance is
// tab-scoped or window-scoped.
BrowserWindowInterface* GetBrowserWindowInterface(
    absl::variant<BrowserWindowInterface*, tabs::TabInterface*> interface) {
  if (absl::holds_alternative<BrowserWindowInterface*>(interface)) {
    return absl::get<BrowserWindowInterface*>(interface);
  }
  return absl::get<tabs::TabInterface*>(interface)->GetBrowserWindowInterface();
}

class HistoryClustersSidePanelContextMenu
    : public ui::SimpleMenuModel,
      public ui::SimpleMenuModel::Delegate {
 public:
  HistoryClustersSidePanelContextMenu(
      absl::variant<BrowserWindowInterface*, tabs::TabInterface*> interface,
      GURL url)
      : ui::SimpleMenuModel(this), interface_(interface), url_(url) {
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
  HistoryClustersSidePanelContextMenu(
      absl::variant<BrowserWindowInterface*, tabs::TabInterface*> interface,
      std::string query)
      : ui::SimpleMenuModel(this), interface_(interface), query_(query) {
    AddItemWithStringId(IDC_CUT, IDS_HISTORY_CLUSTERS_CUT);
    AddItemWithStringId(IDC_COPY, IDS_HISTORY_CLUSTERS_COPY);
    AddItemWithStringId(IDC_PASTE, IDS_HISTORY_CLUSTERS_PASTE);
  }
  ~HistoryClustersSidePanelContextMenu() override = default;

  void ExecuteCommand(int command_id, int event_flags) override {
    switch (command_id) {
      case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB: {
        content::OpenURLParams params(url_, content::Referrer(),
                                      WindowOpenDisposition::NEW_BACKGROUND_TAB,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
        GetBrowserWindowInterface(interface_)
            ->OpenURL(params,
                      /*navigation_handle_callback=*/{});
        break;
      }

      case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW: {
        content::OpenURLParams params(url_, content::Referrer(),
                                      WindowOpenDisposition::NEW_WINDOW,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
        GetBrowserWindowInterface(interface_)
            ->OpenURL(params,
                      /*navigation_handle_callback=*/{});
        break;
      }

      case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD: {
        content::OpenURLParams params(url_, content::Referrer(),
                                      WindowOpenDisposition::OFF_THE_RECORD,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
        GetBrowserWindowInterface(interface_)
            ->OpenURL(params,
                      /*navigation_handle_callback=*/{});
        break;
      }
      case IDC_CONTENT_CONTEXT_COPYLINKLOCATION: {
        ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
        scw.WriteText(base::UTF8ToUTF16(url_.spec()));
        break;
      }
      case IDC_CUT:
        InvokeAction(actions::kActionCut, GetBrowserWindowInterface(interface_)
                                              ->GetActions()
                                              ->root_action_item());
        break;
      case IDC_COPY:
        InvokeAction(actions::kActionCopy, GetBrowserWindowInterface(interface_)
                                               ->GetActions()
                                               ->root_action_item());
        break;
      case IDC_PASTE:
        InvokeAction(actions::kActionPaste,
                     GetBrowserWindowInterface(interface_)
                         ->GetActions()
                         ->root_action_item());
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

 private:
  // Exactly one of `browser_window_interface_` and `tab_interface_` will be
  // non-nullptr.
  absl::variant<BrowserWindowInterface*, tabs::TabInterface*> interface_;
  std::string query_;
  GURL url_;
};

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
  const TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  for (const auto& cluster : clusters_batch) {
    auto cluster_mojom = ClusterToMojom(template_url_service, cluster);
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
    content::WebContents* web_contents,
    BrowserWindowInterface* browser_window_interface)
    : profile_(profile),
      web_contents_(web_contents),
      interface_(browser_window_interface),
      page_handler_(this, std::move(pending_page_handler)) {
  CommonInit();
}

HistoryClustersHandler::HistoryClustersHandler(
    mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents,
    tabs::TabInterface* tab_interface)
    : profile_(profile),
      web_contents_(web_contents),
      interface_(tab_interface),
      page_handler_(this, std::move(pending_page_handler)) {
  CommonInit();
}

void HistoryClustersHandler::CommonInit() {
  DCHECK(profile_);
  DCHECK(web_contents_);

  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(profile_);
  DCHECK(history_clusters_service);
  service_observation_.Observe(history_clusters_service);

  history_service_ = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  browsing_history_service_ = std::make_unique<history::BrowsingHistoryService>(
      this, history_service_, sync_service);
}

HistoryClustersHandler::~HistoryClustersHandler() = default;

void HistoryClustersHandler::SetSidePanelUIEmbedder(
    base::WeakPtr<TopChromeWebUIController::Embedder> side_panel_embedder) {
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
  GetBrowserWindowInterface(interface_)
      ->OpenURL(params,
                /*navigation_handle_callback=*/{});
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

void HistoryClustersHandler::StartQueryClusters(
    const std::string& query,
    std::optional<base::Time> begin_time,
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
  query_clusters_state_ = std::make_unique<QueryClustersState>(
      history_clusters_service->GetWeakPtr(), history_service_, query,
      begin_time.value_or(base::Time()), recluster);
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

void HistoryClustersHandler::HideVisits(std::vector<mojom::URLVisitPtr> visits,
                                        HideVisitsCallback callback) {
  DCHECK(!visits.empty());

  // If there's a pending request, fail because `HistoryClustersHandler` only
  // supports one hide request at a time.
  if (!pending_hide_visits_callback_.is_null() || !history_service_) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::vector<history::VisitID> visit_ids;
  base::ranges::transform(
      visits, std::back_inserter(visit_ids),
      [](const auto& url_visit_ptr) { return url_visit_ptr->visit_id; });

  // Transfer the visits and the callback to member variables.
  pending_hide_visits_ = std::move(visits);
  pending_hide_visits_callback_ = std::move(callback);

  history_service_->HideVisits(
      visit_ids,
      base::BindOnce(&HistoryClustersHandler::OnHideVisitsComplete,
                     weak_ptr_factory_.GetWeakPtr()),
      &pending_hide_visits_task_tracker_);
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
    std::vector<mojom::URLVisitPtr> visits,
    const std::optional<std::string>& tab_group_name) {
  // Hard cap the number of opened visits in a tab group to 32. It's a
  // relatively high cap chosen fairly arbitrarily, because the user took an
  // affirmative action to open this many tabs. And hidden visits aren't opened.
  constexpr size_t kMaxVisitsToOpenInTabGroup = 32;
  if (visits.size() > kMaxVisitsToOpenInTabGroup) {
    visits.resize(kMaxVisitsToOpenInTabGroup);
  }

  auto* model =
      GetBrowserWindowInterface(interface_)->GetFeatures().tab_strip_model();
  std::vector<int> tab_indices;
  tab_indices.reserve(visits.size());
  for (const auto& visit_ptr : visits) {
    auto* opened_web_contents =
        GetBrowserWindowInterface(interface_)
            ->OpenURL(content::OpenURLParams(
                          visit_ptr->normalized_url, content::Referrer(),
                          WindowOpenDisposition::NEW_BACKGROUND_TAB,
                          ui::PAGE_TRANSITION_AUTO_BOOKMARK, false),
                      /*navigation_handle_callback=*/{});

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
  auto new_group_id = model->AddToNewGroup(tab_indices);
  if (!new_group_id.is_empty() && tab_group_name) {
    if (auto* group_model = model->group_model()) {
      auto* tab_group = group_model->GetTabGroup(new_group_id);
      // Copy and modify the existing visual data with a new title.
      tab_groups::TabGroupVisualData visual_data = *tab_group->visual_data();
      visual_data.SetTitle(base::UTF8ToUTF16(*tab_group_name));
      tab_group->SetVisualData(visual_data);
    }
  }
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
}

void HistoryClustersHandler::OnHideVisitsComplete() {
  DCHECK(!pending_hide_visits_callback_.is_null());
  std::move(pending_hide_visits_callback_).Run(/*success=*/true);
  // Notify the page of the successfully hidden visits to update the UI.
  page_->OnVisitsHidden(std::move(pending_hide_visits_));
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

void HistoryClustersHandler::ShowContextMenuForSearchbox(
    const std::string& query,
    const gfx::Point& point) {
  if (history_clusters_side_panel_embedder_) {
    history_clusters_side_panel_embedder_->ShowContextMenu(
        point, std::make_unique<HistoryClustersSidePanelContextMenu>(interface_,
                                                                     query));
  }
}

void HistoryClustersHandler::ShowContextMenuForURL(const GURL& url,
                                                   const gfx::Point& point) {
  if (history_clusters_side_panel_embedder_) {
    history_clusters_side_panel_embedder_->ShowContextMenu(
        point,
        std::make_unique<HistoryClustersSidePanelContextMenu>(interface_, url));
  }
}

}  // namespace history_clusters
