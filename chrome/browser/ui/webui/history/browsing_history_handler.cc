// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/browsing_history_handler.h"

#include <stddef.h>

#include <optional>
#include <set>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/query_parser/snippet.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/webui/resources/cr_components/history/history.mojom.h"

using bookmarks::BookmarkModel;
using history::BrowsingHistoryService;
using history::HistoryService;
using history::WebHistoryService;

namespace {

// Identifiers for the type of device from which a history entry originated.
static const char kDeviceTypeLaptop[] = "laptop";
static const char kDeviceTypePhone[] = "phone";
static const char kDeviceTypeTablet[] = "tablet";

// Gets the name and type of a device for the given sync client ID.
// |name| and |type| are out parameters.
void GetDeviceNameAndType(const syncer::DeviceInfoTracker* tracker,
                          const std::string& client_id,
                          std::string* name,
                          std::string* type) {
  // DeviceInfoTracker must be syncing in order for remote history entries to
  // be available.
  DCHECK(tracker);
  DCHECK(tracker->IsSyncing());

  const syncer::DeviceInfo* device_info = tracker->GetDeviceInfo(client_id);
  if (device_info) {
    *name = device_info->client_name();
    switch (device_info->form_factor()) {
      case syncer::DeviceInfo::FormFactor::kPhone:
        *type = kDeviceTypePhone;
        break;
      case syncer::DeviceInfo::FormFactor::kTablet:
        *type = kDeviceTypeTablet;
        break;
      // return the laptop icon as default.
      case syncer::DeviceInfo::FormFactor::kUnknown:
        [[fallthrough]];
      case syncer::DeviceInfo::FormFactor::kAutomotive:
        [[fallthrough]];
      case syncer::DeviceInfo::FormFactor::kWearable:
        [[fallthrough]];
      case syncer::DeviceInfo::FormFactor::kTv:
        [[fallthrough]];
      case syncer::DeviceInfo::FormFactor::kDesktop:
        *type = kDeviceTypeLaptop;
    }
    return;
  }

  *name = l10n_util::GetStringUTF8(IDS_HISTORY_UNKNOWN_DEVICE);
  *type = kDeviceTypeLaptop;
}

// Formats `entry`'s URL and title and adds them to `result`.
std::pair<std::string, std::string> SetHistoryEntryUrlAndTitle(
    const BrowsingHistoryService::HistoryEntry& entry) {
  bool using_url_as_the_title = false;
  std::u16string title_to_set(entry.title);
  if (entry.title.empty()) {
    using_url_as_the_title = true;
    title_to_set = base::UTF8ToUTF16(entry.url.spec());
  }

  // Since the title can contain BiDi text, we need to mark the text as either
  // RTL or LTR, depending on the characters in the string. If we use the URL
  // as the title, we mark the title as LTR since URLs are always treated as
  // left to right strings.
  if (base::i18n::IsRTL()) {
    if (using_url_as_the_title) {
      base::i18n::WrapStringWithLTRFormatting(&title_to_set);
    } else {
      base::i18n::AdjustStringForLocaleDirection(&title_to_set);
    }
  }

  // Number of chars to truncate titles when making them "short".
  static const size_t kShortTitleLength = 300;
  if (title_to_set.size() > kShortTitleLength) {
    title_to_set.resize(kShortTitleLength);
  }

  return std::make_tuple(entry.url.spec(), base::UTF16ToUTF8(title_to_set));
}

// Helper function to check if entry is present in local database (local-side
// history).
bool IsUrlInLocalDatabase(const BrowsingHistoryService::HistoryEntry& entry) {
  switch (entry.entry_type) {
    case BrowsingHistoryService::HistoryEntry::EntryType::EMPTY_ENTRY:
    case BrowsingHistoryService::HistoryEntry::EntryType::REMOTE_ENTRY:
      return false;
    case BrowsingHistoryService::HistoryEntry::EntryType::LOCAL_ENTRY:
    case BrowsingHistoryService::HistoryEntry::EntryType::COMBINED_ENTRY:
      return true;
  }
  NOTREACHED();
}

// Helper function to check if entry is present in user remote data (server-side
// history).
bool IsEntryInRemoteUserData(
    const BrowsingHistoryService::HistoryEntry& entry) {
  switch (entry.entry_type) {
    case BrowsingHistoryService::HistoryEntry::EntryType::EMPTY_ENTRY:
    case BrowsingHistoryService::HistoryEntry::EntryType::LOCAL_ENTRY:
      return false;
    case BrowsingHistoryService::HistoryEntry::EntryType::REMOTE_ENTRY:
    case BrowsingHistoryService::HistoryEntry::EntryType::COMBINED_ENTRY:
      return true;
  }
  NOTREACHED();
}

// Expected URL types for `UrlIdentity::CreateFromUrl()`.
constexpr UrlIdentity::TypeSet allowed_types = {
    UrlIdentity::Type::kDefault, UrlIdentity::Type::kFile,
    UrlIdentity::Type::kIsolatedWebApp, UrlIdentity::Type::kChromeExtension};
constexpr UrlIdentity::FormatOptions url_identity_options{
    .default_options = {UrlIdentity::DefaultFormatOptions::
                            kOmitSchemePathAndTrivialSubdomains}};

history::mojom::FilteringBehavior FilteringBehaviorToMojom(
    supervised_user::FilteringBehavior filtering_behavior) {
  switch (filtering_behavior) {
    case supervised_user::FilteringBehavior::kAllow:
      return history::mojom::FilteringBehavior::kAllow;
    case supervised_user::FilteringBehavior::kBlock:
      return history::mojom::FilteringBehavior::kBlock;
    case supervised_user::FilteringBehavior::kInvalid:
      return history::mojom::FilteringBehavior::kInvalid;
    default:
      return history::mojom::FilteringBehavior::kUnknown;
  }
}

// Converts `entry` to a history::mojom::QueryResult to be owned by the caller.
history::mojom::HistoryEntryPtr HistoryEntryToMojom(
    const BrowsingHistoryService::HistoryEntry& entry,
    BookmarkModel* bookmark_model,
    Profile& profile,
    const syncer::DeviceInfoTracker* tracker,
    base::Clock* clock) {
  auto result_mojom = history::mojom::HistoryEntry::New();
  base::Value::Dict dictionary;
  auto url_and_title = SetHistoryEntryUrlAndTitle(entry);
  result_mojom->url = url_and_title.first;
  result_mojom->title = url_and_title.second;

  // UrlIdentity holds a user-identifiable string for a URL. We will display
  // this string to the user.
  std::u16string domain =
      UrlIdentity::CreateFromUrl(&profile, entry.url, allowed_types,
                                 url_identity_options)
          .name;

  // When the domain is empty, use the scheme instead. This allows for a
  // sensible treatment of e.g. file: URLs when group by domain is on.
  if (domain.empty()) {
    domain = base::UTF8ToUTF16(entry.url.scheme() + ":");
  }

  // The items which are to be written into result are also described in
  // chrome/browser/resources/history/history.js in @typedef for
  // HistoryEntry. Please update it whenever you add or remove
  // any keys in result.
  result_mojom->domain = base::UTF16ToUTF8(domain);

  result_mojom->fallback_favicon_text =
      base::UTF16ToASCII(favicon::GetFallbackIconText(entry.url));

  result_mojom->time = entry.time.InMillisecondsFSinceUnixEpoch();

  // Pass the timestamps in a list.
  std::vector<double> timestamps;
  for (const base::Time& timestamp : entry.all_timestamps) {
    timestamps.push_back(timestamp.InMillisecondsFSinceUnixEpoch());
  }
  result_mojom->all_timestamps = std::move(timestamps);

  // Always pass the short date since it is needed both in the search and in
  // the monthly view.
  result_mojom->date_short =
      base::UTF16ToUTF8(base::TimeFormatShortDate(entry.time));

  std::u16string snippet_string;
  std::u16string date_relative_day;
  std::u16string date_time_of_day;
  bool is_blocked_visit = false;

  // Only pass in the strings we need (search results need a shortdate
  // and snippet, browse results need day and time information). Makes sure that
  // values of result are never undefined
  if (entry.is_search_result) {
    snippet_string = entry.snippet;
  } else {
    base::Time midnight = clock->Now().LocalMidnight();
    std::u16string date_str =
        ui::TimeFormat::RelativeDate(entry.time, &midnight);
    if (date_str.empty()) {
      date_str = base::TimeFormatFriendlyDate(entry.time);
    } else {
      date_str = l10n_util::GetStringFUTF16(
          IDS_HISTORY_DATE_WITH_RELATIVE_TIME, date_str,
          base::TimeFormatFriendlyDate(entry.time));
    }
    date_relative_day = date_str;
    date_time_of_day = base::TimeFormatTimeOfDay(entry.time);
  }

  std::string device_name;
  std::string device_type;
  if (!entry.client_id.empty()) {
    GetDeviceNameAndType(tracker, entry.client_id, &device_name, &device_type);
  }

  result_mojom->device_name = device_name;
  result_mojom->device_type = device_type;

  supervised_user::FilteringBehavior filtering_behavior;
  if (profile.IsChild()) {
    supervised_user::SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(&profile);
    supervised_user::SupervisedUserURLFilter* url_filter =
        supervised_user_service->GetURLFilter();
    filtering_behavior =
        url_filter->GetFilteringBehavior(entry.url.GetWithEmptyPath()).behavior;
    is_blocked_visit = entry.blocked_visit;
    result_mojom->host_filtering_behavior =
        FilteringBehaviorToMojom(filtering_behavior);
  } else {
    result_mojom->host_filtering_behavior =
        history::mojom::FilteringBehavior::kUnknown;
  }

  result_mojom->date_time_of_day = base::UTF16ToUTF8(date_time_of_day);
  result_mojom->date_relative_day = base::UTF16ToUTF8(date_relative_day);
  result_mojom->snippet = base::UTF16ToUTF8(snippet_string);
  result_mojom->starred = bookmark_model->IsBookmarked(entry.url);
  result_mojom->blocked_visit = is_blocked_visit;
  result_mojom->is_url_in_remote_user_data = IsEntryInRemoteUserData(entry);
  result_mojom->remote_icon_url_for_uma = entry.remote_icon_url_for_uma.spec();

  // Additional debugging fields shown only if the debug feature is enabled.
  if (history_clusters::GetConfig().user_visible_debug) {
    auto debug_mojom = history::mojom::DebugInfo::New();
    debug_mojom->is_url_in_local_database = IsUrlInLocalDatabase(entry);
    debug_mojom->visit_count = entry.visit_count;
    debug_mojom->typed_count = entry.typed_count;
    result_mojom->debug = std::move(debug_mojom);
  }

  return result_mojom;
}

}  // namespace

BrowsingHistoryHandler::BrowsingHistoryHandler(
    mojo::PendingReceiver<history::mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents),
      page_handler_(this, std::move(pending_page_handler)),
      clock_(base::DefaultClock::GetInstance()),
      browsing_history_service_(nullptr) {}

BrowsingHistoryHandler::~BrowsingHistoryHandler() = default;

void BrowsingHistoryHandler::SetSidePanelUIEmbedder(
    base::WeakPtr<TopChromeWebUIController::Embedder> side_panel_embedder) {
  side_panel_embedder_ = side_panel_embedder;
}

void BrowsingHistoryHandler::SetPage(
    mojo::PendingRemote<history::mojom::Page> pending_page) {
  page_.Bind(std::move(pending_page));
  // TODO(mfacey@): Explore whether deferred_callbacks_ can be removed.
  for (auto& deferred_callback : deferred_callbacks_) {
    std::move(deferred_callback).Run();
  }
  deferred_callbacks_.clear();
}

void BrowsingHistoryHandler::ShowSidePanelUI() {
  if (side_panel_embedder_) {
    side_panel_embedder_->ShowUI();
  }
}

void BrowsingHistoryHandler::StartQueryHistory() {
  HistoryService* local_history = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  browsing_history_service_ = std::make_unique<BrowsingHistoryService>(
      this, local_history, sync_service);

  // 150 = RESULTS_PER_PAGE from chrome/browser/resources/history/constants.js
  SendHistoryQuery(150, std::string(), std::nullopt);
}

void BrowsingHistoryHandler::QueryHistory(const std::string& query,
                                          int max_count,
                                          std::optional<double> begin_timestamp,
                                          QueryHistoryCallback callback) {
  if (!browsing_history_service_) {
    // Page was refreshed, so need to call StartQueryHistory here
    StartQueryHistory();
  }

  // Reset the query history continuation callback. Since it is repopulated in
  // OnQueryComplete(), it cannot be reset earlier, as the early return above
  // prevents the QueryHistory() call to the browsing history service.
  query_history_continuation_.Reset();

  // Cancel the previous query if it is still in flight.
  if (query_history_callback_) {
    std::move(query_history_callback_).Run(history::mojom::QueryResult::New());
  }

  query_history_callback_ = std::move(callback);

  SendHistoryQuery(max_count, query, begin_timestamp);
}

void BrowsingHistoryHandler::SendHistoryQuery(
    int max_count,
    const std::string& query,
    std::optional<double> begin_timestamp) {
  history::QueryOptions options;
  options.max_count = max_count;
  options.duplicate_policy = history::QueryOptions::REMOVE_DUPLICATES_PER_DAY;
  std::string query_without_prefix = query;

  const std::string kHostPrefix = "host:";
  if (query.rfind(kHostPrefix, 0) == 0) {
    options.host_only = true;
    query_without_prefix = query.substr(kHostPrefix.length());
  }

  if (begin_timestamp.has_value()) {
    options.begin_time =
        base::Time::FromMillisecondsSinceUnixEpoch(begin_timestamp.value());
  }

  browsing_history_service_->QueryHistory(
      base::UTF8ToUTF16(query_without_prefix), options);
}

void BrowsingHistoryHandler::QueryHistoryContinuation(
    QueryHistoryContinuationCallback callback) {
  // Cancel the previous query if it is still in flight.
  if (query_history_callback_) {
    std::move(query_history_callback_).Run(history::mojom::QueryResult::New());
  }
  query_history_callback_ = std::move(callback);

  if (!query_history_continuation_.is_null()) {
    std::move(query_history_continuation_).Run();
  }
}

void BrowsingHistoryHandler::RemoveVisits(
    const std::vector<history::mojom::RemovalItemPtr> items,
    RemoveVisitsCallback callback) {
  remove_visits_callbacks_.push(std::move(callback));

  std::vector<BrowsingHistoryService::HistoryEntry> items_to_remove;
  items_to_remove.reserve(items.size());
  for (const auto& item : items) {
    const std::string url = item->url;
    const std::vector<double> timestamps = item->timestamps;
    if (url.empty()) {
      NOTREACHED() << "Unable to extract arguments";
    }

    DCHECK_GT(timestamps.size(), 0U);
    BrowsingHistoryService::HistoryEntry entry;
    entry.url = GURL(url);

    for (const auto& timestamp : timestamps) {
      base::Time visit_time =
          base::Time::FromMillisecondsSinceUnixEpoch(timestamp);
      entry.all_timestamps.insert(visit_time);
    }

    items_to_remove.push_back(entry);
  }

  browsing_history_service_->RemoveVisits(items_to_remove);
}

void BrowsingHistoryHandler::OpenClearBrowsingDataDialog() {
  // TODO(beng): This is an improper direct dependency on Browser. Route this
  // through some sort of delegate.
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  chrome::ShowClearBrowsingDataDialog(browser);
}

void BrowsingHistoryHandler::RemoveBookmark(const std::string& url) {
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile_);
  bookmarks::RemoveAllBookmarks(model, GURL(url), FROM_HERE);
}
//
void BrowsingHistoryHandler::SetLastSelectedTab(const int last_tab) {
  profile_->GetPrefs()->SetInteger(history_clusters::prefs::kLastSelectedTab,
                                   last_tab);
}

void BrowsingHistoryHandler::OnQueryComplete(
    const std::vector<BrowsingHistoryService::HistoryEntry>& results,
    const BrowsingHistoryService::QueryResultsInfo& query_results_info,
    base::OnceClosure continuation_closure) {
  query_history_continuation_ = std::move(continuation_closure);
  CHECK(profile_);
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile_);

  const syncer::DeviceInfoTracker* tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile_)
          ->GetDeviceInfoTracker();

  DCHECK(tracker);
  std::vector<history::mojom::HistoryEntryPtr> results_mojom;
  for (const BrowsingHistoryService::HistoryEntry& entry : results) {
    results_mojom.push_back(
        HistoryEntryToMojom(entry, bookmark_model, *profile_, tracker, clock_));
  }

  auto results_info = history::mojom::HistoryQuery::New();
  // The items which are to be written into results_info_ are also
  // described in ui/webui/resources/cr_components/history/history.mojom.
  results_info->term = base::UTF16ToUTF8(query_results_info.search_text);
  results_info->finished = query_results_info.reached_beginning;

  auto final_results = history::mojom::QueryResult::New();
  final_results->info = std::move(results_info);
  final_results->value = std::move(results_mojom);

  std::move(query_history_callback_).Run(std::move(final_results));
  return;
}

void BrowsingHistoryHandler::OnRemoveVisitsComplete() {
  CHECK(!remove_visits_callbacks_.empty());
  std::move(remove_visits_callbacks_.front()).Run();
  remove_visits_callbacks_.pop();
}

void BrowsingHistoryHandler::OnRemoveVisitsFailed() {
  CHECK(!remove_visits_callbacks_.empty());
  std::move(remove_visits_callbacks_.front()).Run();
  remove_visits_callbacks_.pop();
}

void BrowsingHistoryHandler::HistoryDeleted() {
  if (page_) {
    page_->OnHistoryDeleted();
  } else {
    deferred_callbacks_.push_back(base::BindOnce(
        &BrowsingHistoryHandler::HistoryDeleted, weak_factory_.GetWeakPtr()));
  }
}

void BrowsingHistoryHandler::HasOtherFormsOfBrowsingHistory(
    bool has_other_forms,
    bool has_synced_results) {
  if (page_) {
    page_->OnHasOtherFormsChanged(has_other_forms);
  } else {
    deferred_callbacks_.push_back(base::BindOnce(
        &BrowsingHistoryHandler::HasOtherFormsOfBrowsingHistory,
        weak_factory_.GetWeakPtr(), has_other_forms, has_synced_results));
  }
}

Profile* BrowsingHistoryHandler::GetProfile() {
  return profile_;
}
