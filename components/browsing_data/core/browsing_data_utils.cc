// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/browsing_data_utils.h"

#include <optional>
#include <string>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/browsing_data/core/counters/autofill_counter.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/counters/passwords_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace browsing_data {

const char kDeleteBrowsingDataDialogHistogram[] =
    "Privacy.DeleteBrowsingData.Dialog";

// Creates a string like "for a.com, b.com, and 4 more".
std::u16string CreateDomainExamples(
    int password_count,
    const std::vector<std::string> domain_examples) {
  DCHECK_GE(password_count,
            base::checked_cast<browsing_data::BrowsingDataCounter::ResultInt>(
                domain_examples.size()));
  DCHECK_EQ(domain_examples.empty(), password_count == 0);
  std::vector<std::u16string> replacements;

  replacements.emplace_back(base::UTF8ToUTF16(domain_examples[0]));
  if (domain_examples.size() > 1) {
    replacements.emplace_back(base::UTF8ToUTF16(domain_examples[1]));
  }
  if (password_count > 2 && domain_examples.size() > 1) {
    replacements.emplace_back(l10n_util::GetPluralStringFUTF16(
        IDS_DEL_PASSWORDS_COUNTER_AND_X_MORE, password_count - 2));
  }
  std::u16string domains_list = base::ReplaceStringPlaceholders(
      l10n_util::GetPluralStringFUTF16(IDS_DEL_PASSWORDS_DOMAINS_DISPLAY,
                                       (domain_examples.size() > 1)
                                           ? password_count
                                           : domain_examples.size()),
      replacements, nullptr);
  return domains_list;
}

base::Time CalculateBeginDeleteTime(TimePeriod time_period) {
  base::TimeDelta diff;
  base::Time delete_begin_time = base::Time::Now();
  switch (time_period) {
    case TimePeriod::LAST_15_MINUTES:
      diff = base::Minutes(15);
      break;
    case TimePeriod::LAST_HOUR:
      diff = base::Hours(1);
      break;
    case TimePeriod::LAST_DAY:
      diff = base::Hours(24);
      break;
    case TimePeriod::LAST_WEEK:
      diff = base::Hours(7 * 24);
      break;
    case TimePeriod::FOUR_WEEKS:
      diff = base::Hours(4 * 7 * 24);
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
    return base::Time::Now() - base::Days(30);
  }
  return base::Time::Max();
}

void RecordDeletionForPeriod(TimePeriod period) {
  switch (period) {
    case TimePeriod::LAST_15_MINUTES:
      base::RecordAction(
          base::UserMetricsAction("ClearBrowsingData_Last15Minutes"));
      break;
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
    case TimePeriod::LAST_15_MINUTES:
      base::RecordAction(base::UserMetricsAction(
          "ClearBrowsingData_TimePeriodChanged_Last15Minutes"));
      break;
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

void RecordDeleteBrowsingDataAction(DeleteBrowsingDataAction cbd_action) {
  UMA_HISTOGRAM_ENUMERATION("Privacy.DeleteBrowsingData.Action", cbd_action);
}

std::u16string GetCounterTextFromResult(
    const BrowsingDataCounter::Result* result) {
  std::string pref_name = result->source()->GetPrefName();

  if (!result->Finished()) {
    // The counter is still counting.
    return l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_CALCULATING);
  }

  if (pref_name == prefs::kDeletePasswords) {
    const PasswordsCounter::PasswordsResult* password_result =
        static_cast<const PasswordsCounter::PasswordsResult*>(result);

    std::vector<std::u16string> parts;
    BrowsingDataCounter::ResultInt profile_passwords = password_result->Value();

    if (profile_passwords) {
      parts.emplace_back(base::ReplaceStringPlaceholders(
          l10n_util::GetPluralStringFUTF16(
              password_result->is_sync_enabled()
                  ? IDS_DEL_PASSWORDS_COUNTER_SYNCED
                  : IDS_DEL_PASSWORDS_COUNTER,
              profile_passwords),
          {CreateDomainExamples(profile_passwords,
                                password_result->domain_examples())},
          nullptr));
    }

    if (password_result->account_passwords()) {
      parts.emplace_back(base::ReplaceStringPlaceholders(
          l10n_util::GetPluralStringFUTF16(
              IDS_DEL_ACCOUNT_PASSWORDS_COUNTER,
              password_result->account_passwords()),
          {CreateDomainExamples(password_result->account_passwords(),
                                password_result->account_domain_examples())},
          nullptr));
    }

    switch (parts.size()) {
      case 0:
        return l10n_util::GetStringUTF16(
            IDS_DEL_PASSWORDS_AND_SIGNIN_DATA_COUNTER_NONE);
      case 1:
        return parts[0];
      case 2:
        return l10n_util::GetStringFUTF16(
            IDS_DEL_PASSWORDS_AND_SIGNIN_DATA_COUNTER_COMBINATION, parts[0],
            parts[1]);
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  if (pref_name == prefs::kDeleteDownloadHistory) {
    BrowsingDataCounter::ResultInt count =
        static_cast<const BrowsingDataCounter::FinishedResult*>(result)
            ->Value();
    return l10n_util::GetPluralStringFUTF16(IDS_DEL_DOWNLOADS_COUNTER, count);
  }

  if (pref_name == prefs::kDeleteSiteSettings) {
    BrowsingDataCounter::ResultInt count =
        static_cast<const BrowsingDataCounter::FinishedResult*>(result)
            ->Value();
    return l10n_util::GetPluralStringFUTF16(IDS_DEL_SITE_SETTINGS_COUNTER,
                                            count);
  }

  if (pref_name == prefs::kDeleteBrowsingHistoryBasic) {
    // The basic tab doesn't show history counter results.
    NOTREACHED_IN_MIGRATION();
  }

  if (pref_name == prefs::kDeleteBrowsingHistory) {
    // History counter.
    const HistoryCounter::HistoryResult* history_result =
        static_cast<const HistoryCounter::HistoryResult*>(result);
    BrowsingDataCounter::ResultInt local_item_count = history_result->Value();
    bool has_synced_visits = history_result->has_synced_visits();
    return has_synced_visits
               ? l10n_util::GetPluralStringFUTF16(
                     IDS_DEL_BROWSING_HISTORY_COUNTER_SYNCED, local_item_count)
               : l10n_util::GetPluralStringFUTF16(
                     IDS_DEL_BROWSING_HISTORY_COUNTER, local_item_count);
  }

  if (pref_name == prefs::kDeleteFormData) {
    // Autofill counter.
    const AutofillCounter::AutofillResult* autofill_result =
        static_cast<const AutofillCounter::AutofillResult*>(result);
    AutofillCounter::ResultInt num_suggestions = autofill_result->Value();
    AutofillCounter::ResultInt num_credit_cards =
        autofill_result->num_credit_cards();
    AutofillCounter::ResultInt num_addresses = autofill_result->num_addresses();

    std::vector<std::u16string> displayed_strings;

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
          NOTREACHED_IN_MIGRATION();
      }
    }

    bool synced = autofill_result->is_sync_enabled();

    // TODO(crbug.com/371539581): Exclude credit cards from this part, because
    // it can be attributed as "synced", while credit cards are always local.
    std::u16string credit_cards_addresses_autocomplete_entries_part;
    switch (displayed_strings.size()) {
      case 0:
        credit_cards_addresses_autocomplete_entries_part =
            l10n_util::GetStringUTF16(IDS_DEL_AUTOFILL_COUNTER_EMPTY);
        break;
      case 1:
        credit_cards_addresses_autocomplete_entries_part =
            synced ? l10n_util::GetStringFUTF16(
                         IDS_DEL_AUTOFILL_COUNTER_ONE_TYPE_SYNCED,
                         displayed_strings[0])
                   : displayed_strings[0];
        break;
      case 2:
        credit_cards_addresses_autocomplete_entries_part =
            l10n_util::GetStringFUTF16(
                synced ? IDS_DEL_AUTOFILL_COUNTER_TWO_TYPES_SYNCED
                       : IDS_DEL_AUTOFILL_COUNTER_TWO_TYPES,
                displayed_strings[0], displayed_strings[1]);
        break;
      case 3:
        credit_cards_addresses_autocomplete_entries_part =
            l10n_util::GetStringFUTF16(
                synced ? IDS_DEL_AUTOFILL_COUNTER_THREE_TYPES_SYNCED
                       : IDS_DEL_AUTOFILL_COUNTER_THREE_TYPES,
                displayed_strings[0], displayed_strings[1],
                displayed_strings[2]);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }

    AutofillCounter::ResultInt num_user_annotations =
        autofill_result->num_user_annotation_entries();
    if (num_user_annotations) {
      return l10n_util::GetStringFUTF16(
          IDS_DEL_AUTOFILL_SYNCABLE_NON_SYNCABLE_COMBINATION,
          credit_cards_addresses_autocomplete_entries_part,
          l10n_util::GetPluralStringFUTF16(
              IDS_DEL_AUTOFILL_COUNTER_USER_ANNOTATION_ENTRIES,
              num_user_annotations));
    } else {
      return credit_cards_addresses_autocomplete_entries_part;
    }
  }

  NOTREACHED_IN_MIGRATION();
  return std::u16string();
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
      case BrowsingDataType::SITE_DATA:
        *out_pref = prefs::kDeleteCookiesBasic;
        return true;
      case BrowsingDataType::PASSWORDS:
      case BrowsingDataType::FORM_DATA:
      case BrowsingDataType::SITE_SETTINGS:
      case BrowsingDataType::DOWNLOADS:
      case BrowsingDataType::HOSTED_APPS_DATA:
      case BrowsingDataType::TABS:
        return false;  // No corresponding preference on basic tab.
    }
  }
  switch (data_type) {
    case BrowsingDataType::HISTORY:
      *out_pref = prefs::kDeleteBrowsingHistory;
      return true;
    case BrowsingDataType::CACHE:
      *out_pref = prefs::kDeleteCache;
      return true;
    case BrowsingDataType::SITE_DATA:
      *out_pref = prefs::kDeleteCookies;
      return true;
    case BrowsingDataType::PASSWORDS:
      *out_pref = prefs::kDeletePasswords;
      return true;
    case BrowsingDataType::FORM_DATA:
      *out_pref = prefs::kDeleteFormData;
      return true;
    case BrowsingDataType::SITE_SETTINGS:
      *out_pref = prefs::kDeleteSiteSettings;
      return true;
    case BrowsingDataType::DOWNLOADS:
      *out_pref = prefs::kDeleteDownloadHistory;
      return true;
    case BrowsingDataType::HOSTED_APPS_DATA:
      *out_pref = prefs::kDeleteHostedAppsData;
      return true;
    case BrowsingDataType::TABS:
      *out_pref = prefs::kCloseTabs;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

std::optional<BrowsingDataType> GetDataTypeFromDeletionPreference(
    const std::string& pref_name) {
  using DataTypeMap = base::flat_map<std::string, BrowsingDataType>;
  static base::NoDestructor<DataTypeMap> preference_to_datatype(
      std::initializer_list<DataTypeMap::value_type>{
          {prefs::kDeleteBrowsingHistory, BrowsingDataType::HISTORY},
          {prefs::kDeleteBrowsingHistoryBasic, BrowsingDataType::HISTORY},
          {prefs::kDeleteCache, BrowsingDataType::CACHE},
          {prefs::kDeleteCacheBasic, BrowsingDataType::CACHE},
          {prefs::kDeleteCookies, BrowsingDataType::SITE_DATA},
          {prefs::kDeleteCookiesBasic, BrowsingDataType::SITE_DATA},
          {prefs::kDeletePasswords, BrowsingDataType::PASSWORDS},
          {prefs::kDeleteFormData, BrowsingDataType::FORM_DATA},
          {prefs::kDeleteSiteSettings, BrowsingDataType::SITE_SETTINGS},
          {prefs::kDeleteDownloadHistory, BrowsingDataType::DOWNLOADS},
          {prefs::kDeleteHostedAppsData, BrowsingDataType::HOSTED_APPS_DATA},
      });

  auto iter = preference_to_datatype->find(pref_name);
  if (iter != preference_to_datatype->end()) {
    return iter->second;
  }
  return std::nullopt;
}

bool IsHttpsCookieSourceScheme(net::CookieSourceScheme cookie_source_scheme) {
  switch (cookie_source_scheme) {
    case net::CookieSourceScheme::kSecure:
      return true;
    case net::CookieSourceScheme::kNonSecure:
      return false;
    case net::CookieSourceScheme::kUnset:
      // Older cookies don't have a source scheme. Associate them with https
      // since the majority of pageloads are https.
      return true;
  }
}

}  // namespace browsing_data
