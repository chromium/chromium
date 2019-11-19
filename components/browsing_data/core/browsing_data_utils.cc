// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/browsing_data_utils.h"

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "components/browsing_data/core/counters/autofill_counter.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/counters/passwords_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace browsing_data {

base::Time CalculateBeginDeleteTime(TimePeriod time_period) {
  base::TimeDelta diff;
  base::Time delete_begin_time = base::Time::Now();
  switch (time_period) {
    case TimePeriod::LAST_HOUR:
      diff = base::TimeDelta::FromHours(1);
      break;
    case TimePeriod::LAST_DAY:
      diff = base::TimeDelta::FromHours(24);
      break;
    case TimePeriod::LAST_WEEK:
      diff = base::TimeDelta::FromHours(7 * 24);
      break;
    case TimePeriod::FOUR_WEEKS:
      diff = base::TimeDelta::FromHours(4 * 7 * 24);
      break;
    case TimePeriod::ALL_TIME:
    case TimePeriod::OLDER_THAN_30_DAYS:
      delete_begin_time = base::Time();
      break;
  }
  return delete_begin_time - diff;
}

base::Time CalculateEndDeleteTime(TimePeriod time_period) {
  if (time_period == TimePeriod::OLDER_THAN_30_DAYS) {
    return base::Time::Now() - base::TimeDelta::FromDays(30);
  }
  return base::Time::Max();
}

void RecordDeletionForPeriod(TimePeriod period) {
  switch (period) {
    case TimePeriod::LAST_HOUR:
      base::RecordAction(base::UserMetricsAction("ClearBrowsingData_LastHour"));
      break;
    case TimePeriod::LAST_DAY:
      base::RecordAction(base::UserMetricsAction("ClearBrowsingData_LastDay"));
      break;
    case TimePeriod::LAST_WEEK:
      base::RecordAction(base::UserMetricsAction("ClearBrowsingData_LastWeek"));
      break;
    case TimePeriod::FOUR_WEEKS:
      base::RecordAction(
          base::UserMetricsAction("ClearBrowsingData_LastMonth"));
      break;
    case TimePeriod::ALL_TIME:
      base::RecordAction(
          base::UserMetricsAction("ClearBrowsingData_Everything"));
      break;
    case TimePeriod::OLDER_THAN_30_DAYS:
      base::RecordAction(
          base::UserMetricsAction("ClearBrowsingData_OlderThan30Days"));
      break;
  }
}

void RecordTimePeriodChange(TimePeriod period) {
  switch (period) {
    case TimePeriod::LAST_HOUR:
      base::RecordAction(base::UserMetricsAction(
          "ClearBrowsingData_TimePeriodChanged_LastHour"));
      break;
    case TimePeriod::LAST_DAY:
      base::RecordAction(base::UserMetricsAction(
          "ClearBrowsingData_TimePeriodChanged_LastDay"));
      break;
    case TimePeriod::LAST_WEEK:
      base::RecordAction(base::UserMetricsAction(
          "ClearBrowsingData_TimePeriodChanged_LastWeek"));
      break;
    case TimePeriod::FOUR_WEEKS:
      base::RecordAction(base::UserMetricsAction(
          "ClearBrowsingData_TimePeriodChanged_LastMonth"));
      break;
    case TimePeriod::ALL_TIME:
      base::RecordAction(base::UserMetricsAction(
          "ClearBrowsingData_TimePeriodChanged_Everything"));
      break;
    case TimePeriod::OLDER_THAN_30_DAYS:
      base::RecordAction(base::UserMetricsAction(
          "ClearBrowsingData_TimePeriodChanged_OlderThan30Days"));
      break;
  }
}

base::string16 GetCounterTextFromResult(
    const BrowsingDataCounter::Result* result) {
  base::string16 text;
  std::string pref_name = result->source()->GetPrefName();

  if (!result->Finished()) {
    // The counter is still counting.
    text = l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_CALCULATING);

  } else if (pref_name == prefs::kDeletePasswords) {
    const PasswordsCounter::PasswordsResult* password_result =
        static_cast<const PasswordsCounter::PasswordsResult*>(result);

    BrowsingDataCounter::ResultInt password_count = password_result->Value();
    const std::vector<std::string>& domain_examples =
        password_result->domain_examples();

    DCHECK_GE(password_count,
              base::checked_cast<browsing_data::BrowsingDataCounter::ResultInt>(
                  domain_examples.size()));
    DCHECK_EQ(domain_examples.empty(), password_count == 0);

    std::vector<base::string16> replacements;
    if (domain_examples.size()) {
      replacements.emplace_back(base::UTF8ToUTF16(domain_examples[0]));
      if (domain_examples.size() > 1) {
        replacements.emplace_back(base::UTF8ToUTF16(domain_examples[1]));
      }
      if (password_count > 2 && domain_examples.size() > 1) {
        replacements.emplace_back(l10n_util::GetPluralStringFUTF16(
            IDS_DEL_PASSWORDS_COUNTER_AND_X_MORE, password_count - 2));
      }
      const base::string16& domains_list = base::ReplaceStringPlaceholders(
          l10n_util::GetPluralStringFUTF16(IDS_DEL_PASSWORDS_DOMAINS_DISPLAY,
                                           (domain_examples.size() > 1)
                                               ? password_count
                                               : domain_examples.size()),
          replacements, nullptr);
      text = base::ReplaceStringPlaceholders(
          l10n_util::GetPluralStringFUTF16(
              password_result->is_sync_enabled()
                  ? IDS_DEL_PASSWORDS_COUNTER_SYNCED
                  : IDS_DEL_PASSWORDS_COUNTER,
              password_count),
          {domains_list}, nullptr);
    } else {
      text = l10n_util::GetStringUTF16(
          IDS_DEL_PASSWORDS_AND_SIGNIN_DATA_COUNTER_NONE);
    }

  } else if (pref_name == prefs::kDeleteDownloadHistory) {
    BrowsingDataCounter::ResultInt count =
        static_cast<const BrowsingDataCounter::FinishedResult*>(result)
            ->Value();
    text = l10n_util::GetPluralStringFUTF16(IDS_DEL_DOWNLOADS_COUNTER, count);
  } else if (pref_name == prefs::kDeleteSiteSettings) {
    BrowsingDataCounter::ResultInt count =
        static_cast<const BrowsingDataCounter::FinishedResult*>(result)
            ->Value();
    text =
        l10n_util::GetPluralStringFUTF16(IDS_DEL_SITE_SETTINGS_COUNTER, count);
  } else if (pref_name == prefs::kDeleteBrowsingHistoryBasic) {
    // The basic tab doesn't show history counter results.
    NOTREACHED();
  } else if (pref_name == prefs::kDeleteBrowsingHistory) {
    // History counter.
    const HistoryCounter::HistoryResult* history_result =
        static_cast<const HistoryCounter::HistoryResult*>(result);
    BrowsingDataCounter::ResultInt local_item_count = history_result->Value();
    bool has_synced_visits = history_result->has_synced_visits();
    text = has_synced_visits
               ? l10n_util::GetPluralStringFUTF16(
                     IDS_DEL_BROWSING_HISTORY_COUNTER_SYNCED, local_item_count)
               : l10n_util::GetPluralStringFUTF16(
                     IDS_DEL_BROWSING_HISTORY_COUNTER, local_item_count);

  } else if (pref_name == prefs::kDeleteFormData) {
    // Autofill counter.
    const AutofillCounter::AutofillResult* autofill_result =
        static_cast<const AutofillCounter::AutofillResult*>(result);
    AutofillCounter::ResultInt num_suggestions = autofill_result->Value();
    AutofillCounter::ResultInt num_credit_cards =
        autofill_result->num_credit_cards();
    AutofillCounter::ResultInt num_addresses = autofill_result->num_addresses();

    std::vector<base::string16> displayed_strings;

    if (num_credit_cards) {
      displayed_strings.push_back(l10n_util::GetPluralStringFUTF16(
          IDS_DEL_AUTOFILL_COUNTER_CREDIT_CARDS, num_credit_cards));
    }
    if (num_addresses) {
      displayed_strings.push_back(l10n_util::GetPluralStringFUTF16(
          IDS_DEL_AUTOFILL_COUNTER_ADDRESSES, num_addresses));
    }
    if (num_suggestions) {
      // We use a different wording for autocomplete suggestions based on the
      // length of the entire string.
      switch (displayed_strings.size()) {
        case 0:
          displayed_strings.push_back(l10n_util::GetPluralStringFUTF16(
              IDS_DEL_AUTOFILL_COUNTER_SUGGESTIONS, num_suggestions));
          break;
        case 1:
          displayed_strings.push_back(l10n_util::GetPluralStringFUTF16(
              IDS_DEL_AUTOFILL_COUNTER_SUGGESTIONS_LONG, num_suggestions));
          break;
        case 2:
          displayed_strings.push_back(l10n_util::GetPluralStringFUTF16(
              IDS_DEL_AUTOFILL_COUNTER_SUGGESTIONS_SHORT, num_suggestions));
          break;
        default:
          NOTREACHED();
      }
    }

    bool synced = autofill_result->is_sync_enabled();

    // Construct the resulting string from the sections in |displayed_strings|.
    switch (displayed_strings.size()) {
      case 0:
        text = l10n_util::GetStringUTF16(IDS_DEL_AUTOFILL_COUNTER_EMPTY);
        break;
      case 1:
        text = synced ? l10n_util::GetStringFUTF16(
                            IDS_DEL_AUTOFILL_COUNTER_ONE_TYPE_SYNCED,
                            displayed_strings[0])
                      : displayed_strings[0];
        break;
      case 2:
        text = l10n_util::GetStringFUTF16(
            synced ? IDS_DEL_AUTOFILL_COUNTER_TWO_TYPES_SYNCED
                   : IDS_DEL_AUTOFILL_COUNTER_TWO_TYPES,
            displayed_strings[0], displayed_strings[1]);
        break;
      case 3:
        text = l10n_util::GetStringFUTF16(
            synced ? IDS_DEL_AUTOFILL_COUNTER_THREE_TYPES_SYNCED
                   : IDS_DEL_AUTOFILL_COUNTER_THREE_TYPES,
            displayed_strings[0], displayed_strings[1], displayed_strings[2]);
        break;
      default:
        NOTREACHED();
    }
  }

  return text;
}

const char* GetTimePeriodPreferenceName(
    ClearBrowsingDataTab clear_browsing_data_tab) {
  return clear_browsing_data_tab == ClearBrowsingDataTab::BASIC
             ? prefs::kDeleteTimePeriodBasic
             : prefs::kDeleteTimePeriod;
}

bool GetDeletionPreferenceFromDataType(
    BrowsingDataType data_type,
    ClearBrowsingDataTab clear_browsing_data_tab,
    std::string* out_pref) {
  if (clear_browsing_data_tab == ClearBrowsingDataTab::BASIC) {
    switch (data_type) {
      case BrowsingDataType::HISTORY:
        *out_pref = prefs::kDeleteBrowsingHistoryBasic;
        return true;
      case BrowsingDataType::CACHE:
        *out_pref = prefs::kDeleteCacheBasic;
        return true;
      case BrowsingDataType::COOKIES:
        *out_pref = prefs::kDeleteCookiesBasic;
        return true;
      case BrowsingDataType::PASSWORDS:
      case BrowsingDataType::FORM_DATA:
      case BrowsingDataType::BOOKMARKS:
      case BrowsingDataType::SITE_SETTINGS:
      case BrowsingDataType::DOWNLOADS:
      case BrowsingDataType::HOSTED_APPS_DATA:
        return false;  // No corresponding preference on basic tab.
      case BrowsingDataType::NUM_TYPES:
        // This is not an actual type.
        NOTREACHED();
        return false;
    }
  }
  switch (data_type) {
    case BrowsingDataType::HISTORY:
      *out_pref = prefs::kDeleteBrowsingHistory;
      return true;
    case BrowsingDataType::CACHE:
      *out_pref = prefs::kDeleteCache;
      return true;
    case BrowsingDataType::COOKIES:
      *out_pref = prefs::kDeleteCookies;
      return true;
    case BrowsingDataType::PASSWORDS:
      *out_pref = prefs::kDeletePasswords;
      return true;
    case BrowsingDataType::FORM_DATA:
      *out_pref = prefs::kDeleteFormData;
      return true;
    case BrowsingDataType::BOOKMARKS:
      // Bookmarks are deleted on the Android side. No corresponding deletion
      // preference. Not implemented on Desktop.
      return false;
    case BrowsingDataType::SITE_SETTINGS:
      *out_pref = prefs::kDeleteSiteSettings;
      return true;
    case BrowsingDataType::DOWNLOADS:
      *out_pref = prefs::kDeleteDownloadHistory;
      return true;
    case BrowsingDataType::HOSTED_APPS_DATA:
      *out_pref = prefs::kDeleteHostedAppsData;
      return true;
    case BrowsingDataType::NUM_TYPES:
      NOTREACHED();  // This is not an actual type.
      return false;
  }
  NOTREACHED();
  return false;
}

BrowsingDataType GetDataTypeFromDeletionPreference(
    const std::string& pref_name) {
  using DataTypeMap = base::flat_map<std::string, BrowsingDataType>;
  static base::NoDestructor<DataTypeMap> preference_to_datatype(
      std::initializer_list<DataTypeMap::value_type>{
          {prefs::kDeleteBrowsingHistory, BrowsingDataType::HISTORY},
          {prefs::kDeleteBrowsingHistoryBasic, BrowsingDataType::HISTORY},
          {prefs::kDeleteCache, BrowsingDataType::CACHE},
          {prefs::kDeleteCacheBasic, BrowsingDataType::CACHE},
          {prefs::kDeleteCookies, BrowsingDataType::COOKIES},
          {prefs::kDeleteCookiesBasic, BrowsingDataType::COOKIES},
          {prefs::kDeletePasswords, BrowsingDataType::PASSWORDS},
          {prefs::kDeleteFormData, BrowsingDataType::FORM_DATA},
          {prefs::kDeleteSiteSettings, BrowsingDataType::SITE_SETTINGS},
          {prefs::kDeleteDownloadHistory, BrowsingDataType::DOWNLOADS},
          {prefs::kDeleteHostedAppsData, BrowsingDataType::HOSTED_APPS_DATA},
      });

  auto iter = preference_to_datatype->find(pref_name);
  DCHECK(iter != preference_to_datatype->end());
  return iter->second;
}

}  // namespace browsing_data
