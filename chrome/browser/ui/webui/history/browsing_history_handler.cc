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
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

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
                          const std::string& client_id, std::string* name,
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
void SetHistoryEntryUrlAndTitle(
    const BrowsingHistoryService::HistoryEntry& entry,
    base::Value::Dict* result) {
  result->Set("url", entry.url.spec());

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

  result->Set("title", title_to_set);
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
  NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
  return false;
}

// Expected URL types for `UrlIdentity::CreateFromUrl()`.
constexpr UrlIdentity::TypeSet allowed_types = {
    UrlIdentity::Type::kDefault, UrlIdentity::Type::kFile,
    UrlIdentity::Type::kIsolatedWebApp, UrlIdentity::Type::kChromeExtension};
constexpr UrlIdentity::FormatOptions url_identity_options{
    .default_options = {UrlIdentity::DefaultFormatOptions::
                            kOmitSchemePathAndTrivialSubdomains}};

// Converts `entry` to a base::Value::Dict to be owned by the caller.
base::Value::Dict HistoryEntryToValue(
    const BrowsingHistoryService::HistoryEntry& entry,
    BookmarkModel* bookmark_model, Profile& profile,
    const syncer::DeviceInfoTracker* tracker, base::Clock* clock) {
  base::Value::Dict result;
  SetHistoryEntryUrlAndTitle(entry, &result);

  // UrlIdentity holds a user-identifiable string for a URL. We will display
  // this string to the user.
  std::u16string domain =
      UrlIdentity::CreateFromUrl(&profile, entry.url, allowed_types,
                                 url_identity_options)
          .name;

  // When the domain is empty, use the scheme instead. This allows for a
  // sensible treatment of e.g. file: URLs when group by domain is on.
  if (domain.empty()) domain = base::UTF8ToUTF16(entry.url.scheme() + ":");

  // The items which are to be written into result are also described in
  // chrome/browser/resources/history/history.js in @typedef for
  // HistoryEntry. Please update it whenever you add or remove
  // any keys in result.
  result.Set("domain", domain);

  result.Set("fallbackFaviconText",
             base::UTF16ToASCII(favicon::GetFallbackIconText(entry.url)));

  result.Set("time", entry.time.InMillisecondsFSinceUnixEpoch());

  // Pass the timestamps in a list.
  base::Value::List timestamps;
  for (int64_t timestamp : entry.all_timestamps) {
    timestamps.Append(
        base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(timestamp))
            .InMillisecondsFSinceUnixEpoch());
  }
  result.Set("allTimestamps", std::move(timestamps));

  // Always pass the short date since it is needed both in the search and in
  // the monthly view.
  result.Set("dateShort", base::TimeFormatShortDate(entry.time));

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
  result.Set("deviceName", device_name);
  result.Set("deviceType", device_type);

  if (profile.IsChild()) {
    supervised_user::SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(&profile);
    supervised_user::SupervisedUserURLFilter* url_filter =
        supervised_user_service->GetURLFilter();
    supervised_user::FilteringBehavior filtering_behavior =
        url_filter->GetFilteringBehaviorForURL(entry.url.GetWithEmptyPath());
    is_blocked_visit = entry.blocked_visit;
    host_filtering_behavior = static_cast<int>(filtering_behavior);
  }

  result.Set("dateTimeOfDay", date_time_of_day);
  result.Set("dateRelativeDay", date_relative_day);
  result.Set("snippet", snippet_string);
  result.Set("starred", bookmark_model->IsBookmarked(entry.url));
  result.Set("hostFilteringBehavior", host_filtering_behavior);
  result.Set("blockedVisit", is_blocked_visit);
  result.Set("isUrlInRemoteUserData", IsEntryInRemoteUserData(entry));
  result.Set("remoteIconUrlForUma", entry.remote_icon_url_for_uma.spec());

  // Additional debugging fields shown only if the debug feature is enabled.
  if (history_clusters::GetConfig().user_visible_debug) {
    base::Value::Dict debug;
    debug.Set("isUrlInLocalDatabase", IsUrlInLocalDatabase(entry));
    debug.Set("visitCount", entry.visit_count);
    debug.Set("typedCount", entry.typed_count);
    result.Set("debug", std::move(debug));
  }

  return result;
}

}  // namespace

BrowsingHistoryHandler::BrowsingHistoryHandler()
    : clock_(base::DefaultClock::GetInstance()),
      browsing_history_service_(nullptr) {}

BrowsingHistoryHandler::~BrowsingHistoryHandler() = default;

void BrowsingHistoryHandler::OnJavascriptAllowed() {
  if (!browsing_history_service_ && !initial_results_) {
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
  initial_results_ = std::nullopt;
  deferred_callbacks_.clear();
  query_history_callback_id_.clear();
  while (!remove_visits_callbacks_.empty()) {
    remove_visits_callbacks_.pop();
  }
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
  web_ui()->RegisterMessageCallback(
      "setLastSelectedTab",
      base::BindRepeating(&BrowsingHistoryHandler::HandleSetLastSelectedTab,
                          base::Unretained(this)));
}

void BrowsingHistoryHandler::StartQueryHistory() {
  Profile* profile = GetProfile();
  HistoryService* local_history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  browsing_history_service_ = std::make_unique<BrowsingHistoryService>(
      this, local_history, sync_service);

  // 150 = RESULTS_PER_PAGE from chrome/browser/resources/history/constants.js
  SendHistoryQuery(150, std::u16string(), std::nullopt);
}

void BrowsingHistoryHandler::HandleQueryHistory(const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  if (initial_results_.has_value()) {
    ResolveJavascriptCallback(callback_id, *initial_results_);
    initial_results_ = std::nullopt;
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
  const base::Value& search_text = args[1];

  const base::Value& count = args[2];
  if (!count.is_int()) {
    NOTREACHED_IN_MIGRATION() << "Failed to convert argument 2.";
    return;
  }

  std::optional<double> begin_timestamp;
  if (args.size() == 4) {
    begin_timestamp = args[3].GetIfDouble();
  }
  SendHistoryQuery(count.GetInt(), base::UTF8ToUTF16(search_text.GetString()),
                   begin_timestamp);
}

void BrowsingHistoryHandler::SendHistoryQuery(
    int max_count, const std::u16string& query,
    std::optional<double> begin_timestamp) {
  history::QueryOptions options;
  options.max_count = max_count;
  options.duplicate_policy = history::QueryOptions::REMOVE_DUPLICATES_PER_DAY;
  std::u16string query_without_prefix = query;

  const std::u16string kHostPrefix = u"host:";
  if (base::StartsWith(query, kHostPrefix)) {
    options.host_only = true;
    query_without_prefix = query.substr(kHostPrefix.length());
  }

  if (begin_timestamp.has_value()) {
    options.begin_time =
        base::Time::FromMillisecondsSinceUnixEpoch(begin_timestamp.value());
  }

  browsing_history_service_->QueryHistory(query_without_prefix, options);
}

void BrowsingHistoryHandler::HandleQueryHistoryContinuation(
    const base::Value::List& args) {
  CHECK_EQ(args.size(), 1U);
  const base::Value& callback_id = args[0];
  // Cancel the previous query if it is still in flight.
  if (!query_history_callback_id_.empty()) {
    RejectJavascriptCallback(base::Value(query_history_callback_id_),
                             base::Value());
  }
  query_history_callback_id_ = callback_id.GetString();

  DCHECK(query_history_continuation_);
  std::move(query_history_continuation_).Run();
}

void BrowsingHistoryHandler::HandleRemoveVisits(const base::Value::List& args) {
  CHECK_EQ(args.size(), 2U);
  const base::Value& callback_id = args[0];
  remove_visits_callbacks_.push(callback_id.GetString());

  std::vector<BrowsingHistoryService::HistoryEntry> items_to_remove;
  const base::Value& items = args[1];
  const base::Value::List& list = items.GetList();
  items_to_remove.reserve(list.size());
  for (size_t i = 0; i < list.size(); ++i) {
    // Each argument is a dictionary with properties "url" and "timestamps".
    if (!list[i].is_dict()) {
      NOTREACHED_IN_MIGRATION() << "Unable to extract arguments";
      return;
    }

    const std::string* url_ptr = list[i].GetDict().FindString("url");
    const base::Value::List* timestamps_ptr =
        list[i].GetDict().FindList("timestamps");
    if (!url_ptr || !timestamps_ptr) {
      NOTREACHED_IN_MIGRATION() << "Unable to extract arguments";
      return;
    }

    DCHECK_GT(timestamps_ptr->size(), 0U);
    BrowsingHistoryService::HistoryEntry entry;
    entry.url = GURL(*url_ptr);

    for (const base::Value& timestamp : *timestamps_ptr) {
      if (!timestamp.is_double() && !timestamp.is_int()) {
        NOTREACHED_IN_MIGRATION() << "Unable to extract visit timestamp.";
        continue;
      }

      base::Time visit_time =
          base::Time::FromMillisecondsSinceUnixEpoch(timestamp.GetDouble());
      entry.all_timestamps.insert(visit_time.ToInternalValue());
    }

    items_to_remove.push_back(entry);
  }

  browsing_history_service_->RemoveVisits(items_to_remove);
}

void BrowsingHistoryHandler::HandleClearBrowsingData(
    const base::Value::List& args) {
  // TODO(beng): This is an improper direct dependency on Browser. Route this
  // through some sort of delegate.
  Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
  chrome::ShowClearBrowsingDataDialog(browser);
}

void BrowsingHistoryHandler::HandleRemoveBookmark(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  std::string url = args[0].GetString();
  Profile* profile = GetProfile();
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);
  bookmarks::RemoveAllBookmarks(model, GURL(url), FROM_HERE);
}

void BrowsingHistoryHandler::HandleSetLastSelectedTab(
    const base::Value::List& args) {
  const base::Value& last_tab = args[0];
  Profile* profile = GetProfile();
  profile->GetPrefs()->SetInteger(history_clusters::prefs::kLastSelectedTab,
                                  last_tab.GetInt());
}

void BrowsingHistoryHandler::OnQueryComplete(
    const std::vector<BrowsingHistoryService::HistoryEntry>& results,
    const BrowsingHistoryService::QueryResultsInfo& query_results_info,
    base::OnceClosure continuation_closure) {
  query_history_continuation_ = std::move(continuation_closure);
  Profile* profile = Profile::FromWebUI(web_ui());
  CHECK(profile);
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);

  const syncer::DeviceInfoTracker* tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();

  // Convert the result vector into a base::Value::List
  DCHECK(tracker);
  base::Value::List results_value;
  for (const BrowsingHistoryService::HistoryEntry& entry : results) {
    results_value.Append(
        HistoryEntryToValue(entry, bookmark_model, *profile, tracker, clock_));
  }

  base::Value::Dict results_info;
  // The items which are to be written into results_info_value_ are also
  // described in chrome/browser/resources/history/history.js in @typedef for
  // HistoryQuery. Please update it whenever you add or remove any keys in
  // results_info_value_.
  results_info.Set("term", query_results_info.search_text);
  results_info.Set("finished", query_results_info.reached_beginning);

  base::Value::Dict final_results;
  final_results.Set("info", std::move(results_info));
  final_results.Set("value", std::move(results_value));

  if (query_history_callback_id_.empty()) {
    // This can happen if JS isn't ready yet when the first query comes back.
    initial_results_ = std::move(final_results);
    return;
  }

  ResolveJavascriptCallback(base::Value(query_history_callback_id_),
                            final_results);
  query_history_callback_id_.clear();
}

void BrowsingHistoryHandler::OnRemoveVisitsComplete() {
  CHECK(!remove_visits_callbacks_.empty());
  ResolveJavascriptCallback(base::Value(remove_visits_callbacks_.front()),
                            base::Value());
  remove_visits_callbacks_.pop();
}

void BrowsingHistoryHandler::OnRemoveVisitsFailed() {
  CHECK(!remove_visits_callbacks_.empty());
  RejectJavascriptCallback(base::Value(remove_visits_callbacks_.front()),
                           base::Value());
  remove_visits_callbacks_.pop();
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
    bool has_other_forms, bool has_synced_results) {
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
