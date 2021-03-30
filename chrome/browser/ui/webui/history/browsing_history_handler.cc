// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/browsing_history_handler.h"

#include <stddef.h>

#include <set>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/feature_list.h"
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
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/query_parser/snippet.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#endif

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

  std::unique_ptr<syncer::DeviceInfo> device_info =
      tracker->GetDeviceInfo(client_id);
  if (device_info.get()) {
    *name = device_info->client_name();
    switch (device_info->device_type()) {
      case sync_pb::SyncEnums::TYPE_PHONE:
        *type = kDeviceTypePhone;
        break;
      case sync_pb::SyncEnums::TYPE_TABLET:
        *type = kDeviceTypeTablet;
        break;
      default:
        *type = kDeviceTypeLaptop;
    }
    return;
  }

  *name = l10n_util::GetStringUTF8(IDS_HISTORY_UNKNOWN_DEVICE);
  *type = kDeviceTypeLaptop;
}

// Formats |entry|'s URL and title and adds them to |result|.
void SetHistoryEntryUrlAndTitle(
    const BrowsingHistoryService::HistoryEntry& entry,
    base::Value* result) {
  result->SetStringKey("url", entry.url.spec());

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
    if (using_url_as_the_title)
      base::i18n::WrapStringWithLTRFormatting(&title_to_set);
    else
      base::i18n::AdjustStringForLocaleDirection(&title_to_set);
  }

  // Number of chars to truncate titles when making them "short".
  static const size_t kShortTitleLength = 300;
  if (title_to_set.size() > kShortTitleLength)
    title_to_set.resize(kShortTitleLength);

  result->SetStringKey("title", title_to_set);
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
  return false;
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
  return false;
}

// Converts |entry| to a base::Value to be owned by the caller.
base::Value HistoryEntryToValue(
    const BrowsingHistoryService::HistoryEntry& entry,
    BookmarkModel* bookmark_model,
    Profile* profile,
    const syncer::DeviceInfoTracker* tracker,
    base::Clock* clock) {
  base::Value result(base::Value::Type::DICTIONARY);
  SetHistoryEntryUrlAndTitle(entry, &result);

  std::u16string domain = url_formatter::IDNToUnicode(entry.url.host());
  // When the domain is empty, use the scheme instead. This allows for a
  // sensible treatment of e.g. file: URLs when group by domain is on.
  if (domain.empty())
    domain = base::UTF8ToUTF16(entry.url.scheme() + ":");

  // The items which are to be written into result are also described in
  // chrome/browser/resources/history/history.js in @typedef for
  // HistoryEntry. Please update it whenever you add or remove
  // any keys in result.
  result.SetStringKey("domain", domain);

  result.SetStringKey(
      "fallbackFaviconText",
      base::UTF16ToASCII(favicon::GetFallbackIconText(entry.url)));

  result.SetDoubleKey("time", entry.time.ToJsTime());

  // Pass the timestamps in a list.
  base::Value timestamps(base::Value::Type::LIST);
  for (int64_t timestamp : entry.all_timestamps) {
    timestamps.Append(base::Time::FromInternalValue(timestamp).ToJsTime());
  }
  result.SetKey("allTimestamps", std::move(timestamps));

  // Always pass the short date since it is needed both in the search and in
  // the monthly view.
  result.SetStringKey("dateShort", base::TimeFormatShortDate(entry.time));

  std::u16string snippet_string;
  std::u16string date_relative_day;
  std::u16string date_time_of_day;
  bool is_blocked_visit = false;
  int host_filtering_behavior = -1;

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
  if (!entry.client_id.empty())
    GetDeviceNameAndType(tracker, entry.client_id, &device_name, &device_type);
  result.SetStringKey("deviceName", device_name);
  result.SetStringKey("deviceType", device_type);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  SupervisedUserService* supervised_user_service = nullptr;
  if (profile->IsSupervised()) {
    supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile);
  }
  if (supervised_user_service) {
    const SupervisedUserURLFilter* url_filter =
        supervised_user_service->GetURLFilter();
    int filtering_behavior =
        url_filter->GetFilteringBehaviorForURL(entry.url.GetWithEmptyPath());
    is_blocked_visit = entry.blocked_visit;
    host_filtering_behavior = filtering_behavior;
  }
#endif

  result.SetStringKey("dateTimeOfDay", date_time_of_day);
  result.SetStringKey("dateRelativeDay", date_relative_day);
  result.SetStringKey("snippet", snippet_string);
  result.SetBoolKey("starred", bookmark_model->IsBookmarked(entry.url));
  result.SetIntKey("hostFilteringBehavior", host_filtering_behavior);
  result.SetBoolKey("blockedVisit", is_blocked_visit);
  result.SetBoolKey("isUrlInRemoteUserData", IsEntryInRemoteUserData(entry));
  result.SetStringKey("remoteIconUrlForUma",
                      entry.remote_icon_url_for_uma.spec());

  // Additional debugging fields that are only shown if the memories::kDebug
  // feature is enabled.
  if (base::FeatureList::IsEnabled(memories::kDebug)) {
    base::Value debug(base::Value::Type::DICTIONARY);
    debug.SetBoolKey("isUrlInLocalDatabase", IsUrlInLocalDatabase(entry));
    debug.SetIntKey("visitCount", entry.visit_count);
    debug.SetIntKey("typedCount", entry.typed_count);
    result.SetKey("debug", std::move(debug));
  }

  return result;
}

}  // namespace

BrowsingHistoryHandler::BrowsingHistoryHandler()
    : clock_(base::DefaultClock::GetInstance()),
      browsing_history_service_(nullptr) {}

BrowsingHistoryHandler::~BrowsingHistoryHandler() {}

void BrowsingHistoryHandler::OnJavascriptAllowed() {
  if (!browsing_history_service_ && initial_results_.is_none()) {
    // Page was refreshed, so need to call StartQueryHistory here
    StartQueryHistory();
  }

  for (auto& callback : deferred_callbacks_) {
    std::move(callback).Run();
  }
  deferred_callbacks_.clear();
}

void BrowsingHistoryHandler::OnJavascriptDisallowed() {
  weak_factory_.InvalidateWeakPtrs();
  browsing_history_service_ = nullptr;
  initial_results_ = base::Value();
  deferred_callbacks_.clear();
  query_history_callback_id_.clear();
  remove_visits_callback_.clear();
}

void BrowsingHistoryHandler::RegisterMessages() {
  // Create our favicon data source.
  Profile* profile = GetProfile();
  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  web_ui()->RegisterMessageCallback(
      "queryHistory",
      base::BindRepeating(&BrowsingHistoryHandler::HandleQueryHistory,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "queryHistoryContinuation",
      base::BindRepeating(
          &BrowsingHistoryHandler::HandleQueryHistoryContinuation,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeVisits",
      base::BindRepeating(&BrowsingHistoryHandler::HandleRemoveVisits,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearBrowsingData",
      base::BindRepeating(&BrowsingHistoryHandler::HandleClearBrowsingData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeBookmark",
      base::BindRepeating(&BrowsingHistoryHandler::HandleRemoveBookmark,
                          base::Unretained(this)));
}

void BrowsingHistoryHandler::StartQueryHistory() {
  Profile* profile = GetProfile();
  HistoryService* local_history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  browsing_history_service_ = std::make_unique<BrowsingHistoryService>(
      this, local_history, sync_service);

  // 150 = RESULTS_PER_PAGE from chrome/browser/resources/history/constants.js
  SendHistoryQuery(150, std::u16string());
}

void BrowsingHistoryHandler::HandleQueryHistory(const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];
  if (!initial_results_.is_none()) {
    ResolveJavascriptCallback(callback_id, std::move(initial_results_));
    initial_results_ = base::Value();
    return;
  }

  // Reset the query history continuation callback. Since it is repopulated in
  // OnQueryComplete(), it cannot be reset earlier, as the early return above
  // prevents the QueryHistory() call to the browsing history service.
  query_history_continuation_.Reset();

  // Cancel the previous query if it is still in flight.
  if (!query_history_callback_id_.empty()) {
    RejectJavascriptCallback(base::Value(query_history_callback_id_),
                             base::Value());
  }
  query_history_callback_id_ = callback_id.GetString();

  // Parse the arguments from JavaScript. There are two required arguments:
  // - the text to search for (may be empty)
  // - the maximum number of results to return (may be 0, meaning that there
  //   is no maximum).
  const base::Value& search_text = args->GetList()[1];

  const base::Value& count = args->GetList()[2];
  if (!count.is_int()) {
    NOTREACHED() << "Failed to convert argument 2.";
    return;
  }

  SendHistoryQuery(count.GetInt(), base::UTF8ToUTF16(search_text.GetString()));
}

void BrowsingHistoryHandler::SendHistoryQuery(int max_count,
                                              const std::u16string& query) {
  history::QueryOptions options;
  options.max_count = max_count;
  options.duplicate_policy = history::QueryOptions::REMOVE_DUPLICATES_PER_DAY;
  browsing_history_service_->QueryHistory(query, options);
}

void BrowsingHistoryHandler::HandleQueryHistoryContinuation(
    const base::ListValue* args) {
  CHECK(args->GetList().size() == 1);
  const base::Value& callback_id = args->GetList()[0];
  // Cancel the previous query if it is still in flight.
  if (!query_history_callback_id_.empty()) {
    RejectJavascriptCallback(base::Value(query_history_callback_id_),
                             base::Value());
  }
  query_history_callback_id_ = callback_id.GetString();

  DCHECK(query_history_continuation_);
  std::move(query_history_continuation_).Run();
}

void BrowsingHistoryHandler::HandleRemoveVisits(const base::ListValue* args) {
  CHECK(args->GetList().size() == 2);
  const base::Value& callback_id = args->GetList()[0];
  CHECK(remove_visits_callback_.empty());
  remove_visits_callback_ = callback_id.GetString();

  std::vector<BrowsingHistoryService::HistoryEntry> items_to_remove;
  const base::Value& items = args->GetList()[1];
  base::Value::ConstListView list = items.GetList();
  items_to_remove.reserve(list.size());
  for (size_t i = 0; i < list.size(); ++i) {
    // Each argument is a dictionary with properties "url" and "timestamps".
    if (!list[i].is_dict()) {
      NOTREACHED() << "Unable to extract arguments";
      return;
    }

    const std::string* url_ptr = list[i].FindStringKey("url");
    const base::Value* timestamps_ptr = list[i].FindListKey("timestamps");
    if (!url_ptr || !timestamps_ptr) {
      NOTREACHED() << "Unable to extract arguments";
      return;
    }

    base::Value::ConstListView timestamps = timestamps_ptr->GetList();
    DCHECK_GT(timestamps.size(), 0U);
    BrowsingHistoryService::HistoryEntry entry;
    entry.url = GURL(*url_ptr);

    for (size_t ts_index = 0; ts_index < timestamps.size(); ++ts_index) {
      if (!timestamps[ts_index].is_double() && !timestamps[ts_index].is_int()) {
        NOTREACHED() << "Unable to extract visit timestamp.";
        continue;
      }

      base::Time visit_time =
          base::Time::FromJsTime(timestamps[ts_index].GetDouble());
      entry.all_timestamps.insert(visit_time.ToInternalValue());
    }

    items_to_remove.push_back(entry);
  }

  browsing_history_service_->RemoveVisits(items_to_remove);
}

void BrowsingHistoryHandler::HandleClearBrowsingData(
    const base::ListValue* args) {
  // TODO(beng): This is an improper direct dependency on Browser. Route this
  // through some sort of delegate.
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  chrome::ShowClearBrowsingDataDialog(browser);
}

void BrowsingHistoryHandler::HandleRemoveBookmark(const base::ListValue* args) {
  std::u16string url = ExtractStringValue(args);
  Profile* profile = GetProfile();
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);
  bookmarks::RemoveAllBookmarks(model, GURL(url));
}

void BrowsingHistoryHandler::OnQueryComplete(
    const std::vector<BrowsingHistoryService::HistoryEntry>& results,
    const BrowsingHistoryService::QueryResultsInfo& query_results_info,
    base::OnceClosure continuation_closure) {
  query_history_continuation_ = std::move(continuation_closure);
  Profile* profile = Profile::FromWebUI(web_ui());
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);

  const syncer::DeviceInfoTracker* tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();

  // Convert the result vector into a ListValue.
  DCHECK(tracker);
  base::Value results_value(base::Value::Type::LIST);
  for (const BrowsingHistoryService::HistoryEntry& entry : results) {
    results_value.Append(
        HistoryEntryToValue(entry, bookmark_model, profile, tracker, clock_));
  }

  base::Value results_info(base::Value::Type::DICTIONARY);
  // The items which are to be written into results_info_value_ are also
  // described in chrome/browser/resources/history/history.js in @typedef for
  // HistoryQuery. Please update it whenever you add or remove any keys in
  // results_info_value_.
  results_info.SetStringKey("term", query_results_info.search_text);
  results_info.SetBoolKey("finished", query_results_info.reached_beginning);

  base::Value final_results(base::Value::Type::DICTIONARY);
  final_results.SetKey("info", std::move(results_info));
  final_results.SetKey("value", std::move(results_value));

  if (query_history_callback_id_.empty()) {
    // This can happen if JS isn't ready yet when the first query comes back.
    initial_results_ = std::move(final_results);
    return;
  }

  ResolveJavascriptCallback(base::Value(query_history_callback_id_),
                            std::move(final_results));
  query_history_callback_id_.clear();
}

void BrowsingHistoryHandler::OnRemoveVisitsComplete() {
  CHECK(!remove_visits_callback_.empty());
  ResolveJavascriptCallback(base::Value(remove_visits_callback_),
                            base::Value());
  remove_visits_callback_.clear();
}

void BrowsingHistoryHandler::OnRemoveVisitsFailed() {
  CHECK(!remove_visits_callback_.empty());
  RejectJavascriptCallback(base::Value(remove_visits_callback_), base::Value());
  remove_visits_callback_.clear();
}

void BrowsingHistoryHandler::HistoryDeleted() {
  if (IsJavascriptAllowed()) {
    FireWebUIListener("history-deleted", base::Value());
  } else {
    deferred_callbacks_.push_back(base::BindOnce(
        &BrowsingHistoryHandler::HistoryDeleted, weak_factory_.GetWeakPtr()));
  }
}

void BrowsingHistoryHandler::HasOtherFormsOfBrowsingHistory(
    bool has_other_forms,
    bool has_synced_results) {
  if (IsJavascriptAllowed()) {
    FireWebUIListener("has-other-forms-changed", base::Value(has_other_forms));
  } else {
    deferred_callbacks_.push_back(base::BindOnce(
        &BrowsingHistoryHandler::HasOtherFormsOfBrowsingHistory,
        weak_factory_.GetWeakPtr(), has_other_forms, has_synced_results));
  }
}

Profile* BrowsingHistoryHandler::GetProfile() {
  return Profile::FromWebUI(web_ui());
}
