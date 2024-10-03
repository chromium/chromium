// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_clear_browsing_data_handler.h"

#include <stddef.h>

#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/browsing_data_important_sites_util.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/browsing_data/counters/browsing_data_counter_factory.h"
#include "chrome/browser/browsing_data/counters/browsing_data_counter_utils.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/cookie_or_cache_deletion_choice.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

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

ClearBrowsingDataHandler::ClearBrowsingDataHandler(content::WebUI* webui,
                                                   Profile* profile)
    : profile_(profile),
      sync_service_(SyncServiceFactory::GetForProfile(profile_)),
      show_history_deletion_dialog_(false) {}

ClearBrowsingDataHandler::~ClearBrowsingDataHandler() = default;

void ClearBrowsingDataHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "clearBrowsingData",
      base::BindRepeating(&ClearBrowsingDataHandler::HandleClearBrowsingData,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "initializeClearBrowsingData",
      base::BindRepeating(&ClearBrowsingDataHandler::HandleInitialize,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSyncState",
      base::BindRepeating(&ClearBrowsingDataHandler::HandleGetSyncState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "restartClearBrowsingDataCounters",
      base::BindRepeating(&ClearBrowsingDataHandler::HandleRestartCounters,
                          base::Unretained(this)));
}

void ClearBrowsingDataHandler::OnJavascriptAllowed() {
  if (sync_service_)
    sync_service_observation_.Observe(sync_service_.get());

  dse_service_observation_.Observe(
      TemplateURLServiceFactory::GetForProfile(profile_));

  DCHECK(counters_basic_.empty());
  DCHECK(counters_advanced_.empty());
  for (const std::string& pref : kCounterPrefsBasic) {
    AddCounter(BrowsingDataCounterFactory::GetForProfileAndPref(profile_, pref),
               browsing_data::ClearBrowsingDataTab::BASIC);
  }
  for (const std::string& pref : kCounterPrefsAdvanced) {
    AddCounter(BrowsingDataCounterFactory::GetForProfileAndPref(profile_, pref),
               browsing_data::ClearBrowsingDataTab::ADVANCED);
  }
}

void ClearBrowsingDataHandler::OnJavascriptDisallowed() {
  dse_service_observation_.Reset();
  sync_service_observation_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  counters_basic_.clear();
  counters_advanced_.clear();
}

void ClearBrowsingDataHandler::HandleClearBrowsingDataForTest() {
  // HandleClearBrowsingData takes in a ListValue as its only parameter. The
  // ListValue must contain four values: web_ui callback ID, a list of data
  // types that the user cleared from the clear browsing data UI and time period
  // of the data to be cleared.

  base::Value::List data_types;
  data_types.Append("browser.clear_data.browsing_history");

  base::Value::List list_args;
  list_args.Append("webui_callback_id");
  list_args.Append(std::move(data_types));
  list_args.Append(1);
  HandleClearBrowsingData(list_args);
}

void ClearBrowsingDataHandler::HandleClearBrowsingData(
    const base::Value::List& args_list) {
  CHECK_EQ(3U, args_list.size());
  std::string webui_callback_id = args_list[0].GetString();

  PrefService* prefs = profile_->GetPrefs();
  uint64_t remove_mask = 0;
  uint64_t origin_mask = 0;
  std::vector<BrowsingDataType> data_type_vector;

  CHECK(args_list[1].is_list());
  const base::Value::List& data_type_list = args_list[1].GetList();
  auto* sentiment_service = TrustSafetySentimentServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui()));
  for (const base::Value& type : data_type_list) {
    const std::string pref_name = type.GetString();
    std::optional<BrowsingDataType> data_type =
        browsing_data::GetDataTypeFromDeletionPreference(pref_name);
    CHECK(data_type);
    data_type_vector.push_back(*data_type);

    switch (*data_type) {
      case BrowsingDataType::HISTORY:
        if (prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory))
          remove_mask |= chrome_browsing_data_remover::DATA_TYPE_HISTORY;
        break;
      case BrowsingDataType::DOWNLOADS:
        if (prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory))
          remove_mask |= content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS;
        break;
      case BrowsingDataType::CACHE:
        remove_mask |= content::BrowsingDataRemover::DATA_TYPE_CACHE;
        break;
      case BrowsingDataType::SITE_DATA:
        remove_mask |= chrome_browsing_data_remover::DATA_TYPE_SITE_DATA;
        origin_mask |=
            content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;
        break;
      case BrowsingDataType::PASSWORDS:
        remove_mask |= chrome_browsing_data_remover::DATA_TYPE_PASSWORDS;
        remove_mask |=
            chrome_browsing_data_remover::DATA_TYPE_ACCOUNT_PASSWORDS;
        break;
      case BrowsingDataType::FORM_DATA:
        remove_mask |= chrome_browsing_data_remover::DATA_TYPE_FORM_DATA;
        break;
      case BrowsingDataType::SITE_SETTINGS:
        remove_mask |= chrome_browsing_data_remover::DATA_TYPE_CONTENT_SETTINGS;
        break;
      case BrowsingDataType::HOSTED_APPS_DATA:
        remove_mask |= chrome_browsing_data_remover::DATA_TYPE_SITE_DATA;
        origin_mask |= content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
        break;
      case BrowsingDataType::TABS:
        // Tab closure is not implemented yet.
        NOTIMPLEMENTED();
        break;
    }

    // Inform the T&S sentiment service that this datatype was cleared.
    if (sentiment_service) {
      sentiment_service->ClearedBrowsingData(*data_type);
    }
  }

  base::flat_set<BrowsingDataType> data_types(std::move(data_type_vector));

  // Record the deletion of cookies and cache.
  browsing_data::CookieOrCacheDeletionChoice choice =
      browsing_data::CookieOrCacheDeletionChoice::kNeitherCookiesNorCache;
  if (data_types.find(BrowsingDataType::SITE_DATA) != data_types.end()) {
    choice =
        data_types.find(BrowsingDataType::CACHE) != data_types.end()
            ? browsing_data::CookieOrCacheDeletionChoice::kBothCookiesAndCache
            : browsing_data::CookieOrCacheDeletionChoice::kOnlyCookies;
  } else if (data_types.find(BrowsingDataType::CACHE) != data_types.end()) {
    choice = browsing_data::CookieOrCacheDeletionChoice::kOnlyCache;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "History.ClearBrowsingData.UserDeletedCookieOrCacheFromDialog", choice);

  browsing_data::RecordDeleteBrowsingDataAction(
      browsing_data::DeleteBrowsingDataAction::kClearBrowsingDataDialog);

  std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion>
      scoped_data_deletion;

  // If Sync is running, prevent it from being paused during the operation.
  // However, if Sync is in error, clearing cookies should pause it.
  if (!profile_->IsGuestSession() &&
      GetSyncStatusMessageType(profile_) == SyncStatusMessageType::kSynced) {
    // Settings can not be opened in incognito windows.
    DCHECK(!profile_->IsOffTheRecord());
    scoped_data_deletion = AccountReconcilorFactory::GetForProfile(profile_)
                               ->GetScopedSyncDataDeletion();
  }

  int period_selected = args_list[2].GetInt();

  content::BrowsingDataRemover* remover = profile_->GetBrowsingDataRemover();

  base::OnceCallback<void(uint64_t)> callback =
      base::BindOnce(&ClearBrowsingDataHandler::OnClearingTaskFinished,
                     weak_ptr_factory_.GetWeakPtr(), webui_callback_id,
                     std::move(data_types), std::move(scoped_data_deletion));
  browsing_data::TimePeriod time_period =
      static_cast<browsing_data::TimePeriod>(period_selected);

  browsing_data_important_sites_util::Remove(
      remove_mask, origin_mask, time_period,
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::Mode::kPreserve),
      remover, std::move(callback));
}

void ClearBrowsingDataHandler::OnClearingTaskFinished(
    const std::string& webui_callback_id,
    const base::flat_set<BrowsingDataType>& data_types,
    std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion> deletion,
    uint64_t failed_data_types) {
  PrefService* prefs = profile_->GetPrefs();
  int history_notice_shown_times = prefs->GetInteger(
      browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes);

  // When the deletion is complete, we might show an additional dialog with
  // a notice about other forms of browsing history. This is the case if
  const bool show_history_notice =
      // 1. The dialog is relevant for the user.
      show_history_deletion_dialog_ &&
      // 2. The notice has been shown less than |kMaxTimesHistoryNoticeShown|.
      history_notice_shown_times < kMaxTimesHistoryNoticeShown &&
      // 3. The selected data types contained browsing history.
      data_types.find(BrowsingDataType::HISTORY) != data_types.end();

  if (show_history_notice) {
    // Increment the preference.
    prefs->SetInteger(
        browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes,
        history_notice_shown_times + 1);
  }

  UMA_HISTOGRAM_BOOLEAN(
      "History.ClearBrowsingData.ShownHistoryNoticeAfterClearing",
      show_history_notice);

  bool show_passwords_notice =
      (failed_data_types & chrome_browsing_data_remover::DATA_TYPE_PASSWORDS);

  base::Value::Dict result;
  result.Set("showHistoryNotice", show_history_notice);
  result.Set("showPasswordsNotice", show_passwords_notice);

  if (toast_features::IsEnabled(toast_features::kClearBrowsingDataToast)) {
    tabs::TabInterface* tab =
        tabs::TabInterface::MaybeGetFromContents(web_ui()->GetWebContents());
    if (tab && tab->IsInForeground()) {
      CHECK(tab->GetBrowserWindowInterface());
      ToastController* const toast_controller =
          tab->GetBrowserWindowInterface()->GetFeatures().toast_controller();
      if (toast_controller) {
        toast_controller->MaybeShowToast(
            ToastParams(ToastId::kClearBrowsingData));
      }
    }
  }

  ResolveJavascriptCallback(base::Value(webui_callback_id), result);
}

void ClearBrowsingDataHandler::HandleInitialize(const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];

  // Needed because WebUI doesn't handle renderer crashes. See crbug.com/610450.
  weak_ptr_factory_.InvalidateWeakPtrs();

  UpdateSyncState();
  RefreshHistoryNotice();

  // Restart the counters each time the dialog is reopened.
  //
  // TODO(crbug.com/331925113): Since each Clear Browsing Data dialog execution
  // commits the time selection to prefs, we may read it back from there.
  // However, it would be safer if the "initializeClearBrowsingData" delivered
  // the actual initial selection from the UI.
  PrefService* prefs = profile_->GetPrefs();
  auto initial_period_basic = static_cast<browsing_data::TimePeriod>(
      prefs->GetInteger(browsing_data::prefs::kDeleteTimePeriodBasic));
  auto initial_period_advanced = static_cast<browsing_data::TimePeriod>(
      prefs->GetInteger(browsing_data::prefs::kDeleteTimePeriod));
  RestartCounters(true /* basic */, initial_period_basic);
  RestartCounters(false /* basic */, initial_period_advanced);

  ResolveJavascriptCallback(callback_id, base::Value() /* Promise<void> */);
}

void ClearBrowsingDataHandler::HandleGetSyncState(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, CreateSyncStateEvent());
}

void ClearBrowsingDataHandler::HandleRestartCounters(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  RestartCounters(args[0].GetBool() /* basic */,
                  static_cast<browsing_data::TimePeriod>(args[1].GetInt()));
}

void ClearBrowsingDataHandler::OnStateChanged(syncer::SyncService* sync) {
  UpdateSyncState();
}

void ClearBrowsingDataHandler::UpdateSyncState() {
  FireWebUIListener("update-sync-state", CreateSyncStateEvent());
}

base::Value::Dict ClearBrowsingDataHandler::CreateSyncStateEvent() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  base::Value::Dict event;
  event.Set("signedIn", identity_manager && identity_manager->HasPrimaryAccount(
                                                signin::ConsentLevel::kSignin));
  event.Set("syncConsented",
            identity_manager && identity_manager->HasPrimaryAccount(
                                    signin::ConsentLevel::kSync));
  event.Set("syncingHistory", sync_service_ &&
                                  sync_service_->IsSyncFeatureActive() &&
                                  sync_service_->GetActiveDataTypes().Has(
                                      syncer::HISTORY_DELETE_DIRECTIVES));
  event.Set("shouldShowCookieException",
            browsing_data_counter_utils::ShouldShowCookieException(profile_));

  event.Set("isNonGoogleDse", false);
  const TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* dse = template_url_service->GetDefaultSearchProvider();
  if (dse && dse->GetEngineType(template_url_service->search_terms_data()) !=
                 SearchEngineType::SEARCH_ENGINE_GOOGLE) {
    // Non-Google DSE. Prepopulated DSEs have an ID > 0.
    event.Set("isNonGoogleDse", true);
    event.Set(
        "nonGoogleSearchHistoryString",
        (dse->prepopulate_id() > 0)
            ? l10n_util::GetStringFUTF16(
                  IDS_SETTINGS_CLEAR_NON_GOOGLE_SEARCH_HISTORY_PREPOPULATED_DSE,
                  dse->short_name())
            : l10n_util::GetStringUTF16(
                  IDS_SETTINGS_CLEAR_NON_GOOGLE_SEARCH_HISTORY_NON_PREPOPULATED_DSE));
  }
  return event;
}

void ClearBrowsingDataHandler::RefreshHistoryNotice() {
  // If the dialog with history notice has been shown less than
  // |kMaxTimesHistoryNoticeShown| times, we might have to show it when the
  // user deletes history. Find out if the conditions are met.
  int notice_shown_times = profile_->GetPrefs()->GetInteger(
      browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes);

  if (notice_shown_times < kMaxTimesHistoryNoticeShown) {
    browsing_data::ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
        sync_service_, WebHistoryServiceFactory::GetForProfile(profile_),
        chrome::GetChannel(),
        base::BindOnce(&ClearBrowsingDataHandler::UpdateHistoryDeletionDialog,
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
  counter->InitWithoutPeriodPref(
      profile_->GetPrefs(), tab, base::Time(),
      base::BindRepeating(&ClearBrowsingDataHandler::UpdateCounterText,
                          base::Unretained(this)));

  ((tab == browsing_data::ClearBrowsingDataTab::BASIC) ? counters_basic_
                                                       : counters_advanced_)
      .push_back(std::move(counter));
}

void ClearBrowsingDataHandler::UpdateCounterText(
    std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
  FireWebUIListener(
      "update-counter-text", base::Value(result->source()->GetPrefName()),
      base::Value(browsing_data_counter_utils::GetChromeCounterTextFromResult(
          result.get(), profile_)));
}

void ClearBrowsingDataHandler::RestartCounters(
    bool basic,
    browsing_data::TimePeriod time_period) {
  // Updating the begin time of a counter automatically forces a restart.
  for (const auto& counter : (basic ? counters_basic_ : counters_advanced_)) {
    counter->SetBeginTime(browsing_data::CalculateBeginDeleteTime(time_period));
  }
}

void ClearBrowsingDataHandler::HandleTimePeriodChanged(
    const std::string& pref_name) {
  PrefService* prefs = profile_->GetPrefs();
  int period = prefs->GetInteger(pref_name);

  browsing_data::TimePeriod time_period =
      static_cast<browsing_data::TimePeriod>(period);
  browsing_data::RecordTimePeriodChange(time_period);
}

void ClearBrowsingDataHandler::OnTemplateURLServiceChanged() {
  UpdateSyncState();
}

}  // namespace settings
