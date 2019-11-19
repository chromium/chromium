// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_clear_browsing_data_handler.h"

#include <stddef.h>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_important_sites_util.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/counters/browsing_data_counter_factory.h"
#include "chrome/browser/browsing_data/counters/browsing_data_counter_utils.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/feature_engagement/buildflags.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/text/bytes_formatting.h"

#if BUILDFLAG(ENABLE_LEGACY_DESKTOP_IN_PRODUCT_HELP)
#include "chrome/browser/feature_engagement/incognito_window/incognito_window_tracker.h"
#include "chrome/browser/feature_engagement/incognito_window/incognito_window_tracker_factory.h"
#endif

using BrowsingDataType = browsing_data::BrowsingDataType;

namespace {

const int kMaxTimesHistoryNoticeShown = 1;

// TODO(msramek): Get the list of deletion preferences from the JS side.
const char* kCounterPrefsAdvanced[] = {
    browsing_data::prefs::kDeleteBrowsingHistory,
    browsing_data::prefs::kDeleteCache,
    browsing_data::prefs::kDeleteCookies,
    browsing_data::prefs::kDeleteDownloadHistory,
    browsing_data::prefs::kDeleteFormData,
    browsing_data::prefs::kDeleteHostedAppsData,
    browsing_data::prefs::kDeletePasswords,
    browsing_data::prefs::kDeleteSiteSettings,
};

// Additional counters for the basic tab of CBD.
const char* kCounterPrefsBasic[] = {
    browsing_data::prefs::kDeleteCacheBasic,
};

} // namespace

namespace settings {

// ClearBrowsingDataHandler ----------------------------------------------------

ClearBrowsingDataHandler::ClearBrowsingDataHandler(content::WebUI* webui)
    : profile_(Profile::FromWebUI(webui)),
      sync_service_(ProfileSyncServiceFactory::GetForProfile(profile_)),
      sync_service_observer_(this),
      show_history_deletion_dialog_(false) {}

ClearBrowsingDataHandler::~ClearBrowsingDataHandler() {
}

void ClearBrowsingDataHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "clearBrowsingData",
      base::BindRepeating(&ClearBrowsingDataHandler::HandleClearBrowsingData,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "initializeClearBrowsingData",
      base::BindRepeating(&ClearBrowsingDataHandler::HandleInitialize,
                          base::Unretained(this)));
}

void ClearBrowsingDataHandler::OnJavascriptAllowed() {
  if (sync_service_)
    sync_service_observer_.Add(sync_service_);

  DCHECK(counters_.empty());
  for (const std::string& pref : kCounterPrefsBasic) {
    AddCounter(BrowsingDataCounterFactory::GetForProfileAndPref(profile_, pref),
               browsing_data::ClearBrowsingDataTab::BASIC);
  }
  for (const std::string& pref : kCounterPrefsAdvanced) {
    AddCounter(BrowsingDataCounterFactory::GetForProfileAndPref(profile_, pref),
               browsing_data::ClearBrowsingDataTab::ADVANCED);
  }
  PrefService* prefs = profile_->GetPrefs();
  period_ = std::make_unique<IntegerPrefMember>();
  period_->Init(
      browsing_data::prefs::kDeleteTimePeriod, prefs,
      base::BindRepeating(&ClearBrowsingDataHandler::HandleTimePeriodChanged,
                          base::Unretained(this)));
  periodBasic_ = std::make_unique<IntegerPrefMember>();
  periodBasic_->Init(
      browsing_data::prefs::kDeleteTimePeriodBasic, prefs,
      base::BindRepeating(&ClearBrowsingDataHandler::HandleTimePeriodChanged,
                          base::Unretained(this)));
}

void ClearBrowsingDataHandler::OnJavascriptDisallowed() {
  sync_service_observer_.RemoveAll();
  weak_ptr_factory_.InvalidateWeakPtrs();
  counters_.clear();
  period_.reset();
  periodBasic_.reset();
}

void ClearBrowsingDataHandler::HandleClearBrowsingDataForTest() {
  // HandleClearBrowsingData takes in a ListValue as its only parameter. The
  // ListValue must contain four values: web_ui callback ID, a list of data
  // types that the user cleared from the clear browsing data UI and time period
  // of the data to be cleared.

  std::unique_ptr<base::ListValue> data_types =
      std::make_unique<base::ListValue>();
  data_types->AppendString("browser.clear_data.browsing_history");

  base::ListValue list_args;
  list_args.AppendString("webui_callback_id");
  list_args.Append(std::move(data_types));
  list_args.AppendInteger(1u);
  HandleClearBrowsingData(&list_args);
}

void ClearBrowsingDataHandler::HandleClearBrowsingData(
    const base::ListValue* args) {
  CHECK_EQ(3U, args->GetSize());
  std::string webui_callback_id;
  CHECK(args->GetString(0, &webui_callback_id));

  PrefService* prefs = profile_->GetPrefs();

  int site_data_mask = ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA;
  // Don't try to clear LSO data if it's not supported.
  if (!prefs->GetBoolean(prefs::kClearPluginLSODataEnabled))
    site_data_mask &= ~ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA;

  int remove_mask = 0;
  int origin_mask = 0;
  std::vector<BrowsingDataType> data_type_vector;
  const base::ListValue* data_type_list = nullptr;
  CHECK(args->GetList(1, &data_type_list));
  for (const base::Value& type : *data_type_list) {
    std::string pref_name;
    CHECK(type.GetAsString(&pref_name));
    BrowsingDataType data_type =
        browsing_data::GetDataTypeFromDeletionPreference(pref_name);
    data_type_vector.push_back(data_type);

    switch (data_type) {
      case BrowsingDataType::HISTORY:
        if (prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory))
          remove_mask |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY;
        break;
      case BrowsingDataType::DOWNLOADS:
        if (prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory))
          remove_mask |= content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS;
        break;
      case BrowsingDataType::CACHE:
        remove_mask |= content::BrowsingDataRemover::DATA_TYPE_CACHE;
        break;
      case BrowsingDataType::COOKIES:
        remove_mask |= site_data_mask;
        origin_mask |=
            content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;
        break;
      case BrowsingDataType::PASSWORDS:
        remove_mask |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS;
        break;
      case BrowsingDataType::FORM_DATA:
        remove_mask |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA;
        break;
      case BrowsingDataType::SITE_SETTINGS:
        remove_mask |=
            ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS;
        break;
      case BrowsingDataType::HOSTED_APPS_DATA:
        remove_mask |= site_data_mask;
        origin_mask |= content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
        break;
      case BrowsingDataType::BOOKMARKS:
        // Only implemented on Android.
        NOTREACHED();
        break;
      case BrowsingDataType::NUM_TYPES:
        NOTREACHED();
        break;
    }
  }

  base::flat_set<BrowsingDataType> data_types(std::move(data_type_vector));

  // Record the deletion of cookies and cache.
  content::BrowsingDataRemover::CookieOrCacheDeletionChoice choice =
      content::BrowsingDataRemover::NEITHER_COOKIES_NOR_CACHE;
  if (data_types.find(BrowsingDataType::COOKIES) != data_types.end()) {
    choice = data_types.find(BrowsingDataType::CACHE) != data_types.end()
                 ? content::BrowsingDataRemover::BOTH_COOKIES_AND_CACHE
                 : content::BrowsingDataRemover::ONLY_COOKIES;
  } else if (data_types.find(BrowsingDataType::CACHE) != data_types.end()) {
    choice = content::BrowsingDataRemover::ONLY_CACHE;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "History.ClearBrowsingData.UserDeletedCookieOrCacheFromDialog", choice,
      content::BrowsingDataRemover::MAX_CHOICE_VALUE);

  // Record the circumstances under which passwords are deleted.
  if (data_types.find(BrowsingDataType::PASSWORDS) != data_types.end()) {
    static const BrowsingDataType other_types[] = {
        BrowsingDataType::HISTORY,        BrowsingDataType::DOWNLOADS,
        BrowsingDataType::CACHE,          BrowsingDataType::COOKIES,
        BrowsingDataType::FORM_DATA,      BrowsingDataType::HOSTED_APPS_DATA,
    };
    static size_t num_other_types = base::size(other_types);
    int checked_other_types =
        std::count_if(other_types, other_types + num_other_types,
                      [&data_types](BrowsingDataType type) {
                        return data_types.find(type) != data_types.end();
                      });
    base::UmaHistogramSparse(
        "History.ClearBrowsingData.PasswordsDeletion.AdditionalDatatypesCount",
        checked_other_types);
  }

  std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion>
      scoped_data_deletion;

  // If Sync is running, prevent it from being paused during the operation.
  // However, if Sync is in error, clearing cookies should pause it.
  if (!profile_->IsGuestSession() &&
      sync_ui_util::GetStatus(profile_) == sync_ui_util::SYNCED) {
    // Settings can not be opened in incognito windows.
    DCHECK(!profile_->IsOffTheRecord());
    scoped_data_deletion = AccountReconcilorFactory::GetForProfile(profile_)
                               ->GetScopedSyncDataDeletion();
  }

  int period_selected;
  CHECK(args->GetInteger(2, &period_selected));

  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(profile_);

  base::OnceClosure callback =
      base::BindOnce(&ClearBrowsingDataHandler::OnClearingTaskFinished,
                     weak_ptr_factory_.GetWeakPtr(), webui_callback_id,
                     std::move(data_types), std::move(scoped_data_deletion));
  browsing_data::TimePeriod time_period =
      static_cast<browsing_data::TimePeriod>(period_selected);

  browsing_data_important_sites_util::Remove(
      remove_mask, origin_mask, time_period,
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::BLACKLIST),
      remover, std::move(callback));

#if BUILDFLAG(ENABLE_LEGACY_DESKTOP_IN_PRODUCT_HELP)
  feature_engagement::IncognitoWindowTrackerFactory::GetInstance()
      ->GetForProfile(profile_)
      ->OnBrowsingDataCleared();
#endif
}

void ClearBrowsingDataHandler::OnClearingTaskFinished(
    const std::string& webui_callback_id,
    const base::flat_set<BrowsingDataType>& data_types,
    std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion> deletion) {
  PrefService* prefs = profile_->GetPrefs();
  int notice_shown_times = prefs->GetInteger(
      browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes);

  // When the deletion is complete, we might show an additional dialog with
  // a notice about other forms of browsing history. This is the case if
  const bool show_notice =
      // 1. The dialog is relevant for the user.
      show_history_deletion_dialog_ &&
      // 2. The notice has been shown less than |kMaxTimesHistoryNoticeShown|.
      notice_shown_times < kMaxTimesHistoryNoticeShown &&
      // 3. The selected data types contained browsing history.
      data_types.find(BrowsingDataType::HISTORY) != data_types.end();

  if (show_notice) {
    // Increment the preference.
    prefs->SetInteger(
        browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes,
        notice_shown_times + 1);
  }

  UMA_HISTOGRAM_BOOLEAN(
      "History.ClearBrowsingData.ShownHistoryNoticeAfterClearing", show_notice);

  ResolveJavascriptCallback(base::Value(webui_callback_id),
                            base::Value(show_notice));
}

void ClearBrowsingDataHandler::HandleInitialize(const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  // Needed because WebUI doesn't handle renderer crashes. See crbug.com/610450.
  weak_ptr_factory_.InvalidateWeakPtrs();

  UpdateSyncState();
  RefreshHistoryNotice();

  // Restart the counters each time the dialog is reopened.
  for (const auto& counter : counters_)
    counter->Restart();

  ResolveJavascriptCallback(*callback_id, base::Value() /* Promise<void> */);
}

void ClearBrowsingDataHandler::OnStateChanged(syncer::SyncService* sync) {
  UpdateSyncState();
}

void ClearBrowsingDataHandler::UpdateSyncState() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  FireWebUIListener(
      "update-sync-state",
      base::Value(identity_manager && identity_manager->HasPrimaryAccount()),
      base::Value(sync_service_ && sync_service_->IsSyncFeatureActive() &&
                  sync_service_->GetActiveDataTypes().Has(
                      syncer::HISTORY_DELETE_DIRECTIVES)),
      base::Value(
          browsing_data_counter_utils::ShouldShowCookieException(profile_)));
}

void ClearBrowsingDataHandler::RefreshHistoryNotice() {
  // If the dialog with history notice has been shown less than
  // |kMaxTimesHistoryNoticeShown| times, we might have to show it when the
  // user deletes history. Find out if the conditions are met.
  int notice_shown_times = profile_->GetPrefs()->GetInteger(
      browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes);

  if (notice_shown_times < kMaxTimesHistoryNoticeShown) {
    browsing_data::ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
        sync_service_,
        WebHistoryServiceFactory::GetForProfile(profile_),
        chrome::GetChannel(),
        base::Bind(&ClearBrowsingDataHandler::UpdateHistoryDeletionDialog,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void ClearBrowsingDataHandler::UpdateHistoryDeletionDialog(bool show) {
  // This is used by OnClearingTaskFinished (when the deletion finishes).
  show_history_deletion_dialog_ = show;
}

void ClearBrowsingDataHandler::AddCounter(
    std::unique_ptr<browsing_data::BrowsingDataCounter> counter,
    browsing_data::ClearBrowsingDataTab tab) {
  DCHECK(counter);
  counter->Init(profile_->GetPrefs(), tab,
                base::Bind(&ClearBrowsingDataHandler::UpdateCounterText,
                           base::Unretained(this)));
  counters_.push_back(std::move(counter));
}

void ClearBrowsingDataHandler::UpdateCounterText(
    std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
  FireWebUIListener(
      "update-counter-text", base::Value(result->source()->GetPrefName()),
      base::Value(browsing_data_counter_utils::GetChromeCounterTextFromResult(
          result.get(), profile_)));
}

void ClearBrowsingDataHandler::HandleTimePeriodChanged(
    const std::string& pref_name) {
  PrefService* prefs = profile_->GetPrefs();
  int period = prefs->GetInteger(pref_name);

  browsing_data::TimePeriod time_period =
      static_cast<browsing_data::TimePeriod>(period);
  browsing_data::RecordTimePeriodChange(time_period);
}

}  // namespace settings
