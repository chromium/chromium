// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi/gcapi_omaha_experiment.h"

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/installer/gcapi/gcapi.h"
#include "chrome/installer/gcapi/google_update_util.h"
#include "chrome/installer/util/google_update_constants.h"

namespace {

// Returns the number of weeks since 2/3/2003.
int GetCurrentRlzWeek(const base::Time& current_time) {
  static constexpr base::Time::Exploded kFeb32003 = {
      .year = 2003, .month = 2, .day_of_week = 1, .day_of_month = 3};
  base::Time f;
  bool conversion_success = base::Time::FromUTCExploded(kFeb32003, &f);
  DCHECK(conversion_success);
  base::TimeDelta delta = current_time - f;
  return delta.InDays() / 7;
}

bool SetExperimentLabel(const wchar_t* brand_code,
                        const std::wstring& label,
                        int shell_mode) {
  if (!brand_code) {
    return false;
  }

  const bool system_level = shell_mode == GCAPI_INVOKED_UAC_ELEVATION;

  std::wstring original_labels;
  if (!gcapi_internals::ReadExperimentLabels(system_level, &original_labels))
    return false;

  // Split the original labels by the label separator.
  std::vector<std::wstring> entries = base::SplitString(
      original_labels, std::wstring(1, kExperimentLabelSeparator),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Keep all labels, but the one we want to add/replace.
  std::wstring label_and_separator(label);
  label_and_separator.push_back('=');
  std::wstring new_labels;
  for (const std::wstring& entry : entries) {
    if (!entry.empty() && !base::StartsWith(entry, label_and_separator,
                                            base::CompareCase::SENSITIVE)) {
      new_labels += entry;
      new_labels += kExperimentLabelSeparator;
    }
  }

  new_labels.append(
      gcapi_internals::GetGCAPIExperimentLabel(brand_code, label));

  return gcapi_internals::SetExperimentLabels(system_level, new_labels);
}

}  // namespace

namespace gcapi_internals {

const wchar_t kReactivationLabel[] = L"reacbrand";
const wchar_t kRelaunchLabel[] = L"relaunchbrand";

std::wstring GetGCAPIExperimentLabel(const wchar_t* brand_code,
                                     const std::wstring& label) {
  // Keeps a fixed time state for this GCAPI instance; this makes tests reliable
  // when crossing time boundaries on the system clock and doesn't otherwise
  // affect results of this short lived binary.
  static time_t instance_time_value = 0;
  if (instance_time_value == 0)
    instance_time_value = base::Time::Now().ToTimeT();

  base::Time instance_time = base::Time::FromTimeT(instance_time_value);

  return base::StrCat({label, L"=", brand_code, L"_",
                       base::NumberToWString(GetCurrentRlzWeek(instance_time)),
                       L"|", BuildExperimentDateString(instance_time)});
}

}  // namespace gcapi_internals

const wchar_t kExperimentLabelSeparator = ';';

bool SetReactivationExperimentLabels(const wchar_t* brand_code,
                                     int shell_mode) {
  return SetExperimentLabel(brand_code, gcapi_internals::kReactivationLabel,
                            shell_mode);
}

bool SetRelaunchExperimentLabels(const wchar_t* brand_code, int shell_mode) {
  return SetExperimentLabel(brand_code, gcapi_internals::kRelaunchLabel,
                            shell_mode);
}

std::wstring BuildExperimentDateString(base::Time current_time) {
  // It's not critical that we deal with leap years etc.; approximating one year
  // as 365 days is fine.
  current_time += base::Days(365);

  // The Google Update experiment_labels timestamp format is:
  // "DAY, DD0 MON YYYY HH0:MI0:SE0 TZ"
  //  DAY = 3 character day of week,
  //  DD0 = 2 digit day of month,
  //  MON = 3 character month of year,
  //  YYYY = 4 digit year,
  //  HH0 = 2 digit hour,
  //  MI0 = 2 digit minute,
  //  SE0 = 2 digit second,
  //  TZ = 3 character timezone
  // Note that this cannot use base/i18n/time_formatting.h, since it is part of
  // a standalone DLL that third parties may use without necessarily
  // initializing ICU.
  static constexpr char kDays[7][4] = {"Sun", "Mon", "Tue", "Wed",
                                       "Thu", "Fri", "Sat"};
  static constexpr char kMonths[12][4] = {"Jan", "Feb", "Mar", "Apr",
                                          "May", "Jun", "Jul", "Aug",
                                          "Sep", "Oct", "Nov", "Dec"};
  base::Time::Exploded then;
  current_time.UTCExplode(&then);
  return base::ASCIIToWide(base::StringPrintf(
      "%s, %02d %s %d %02d:%02d:%02d GMT", kDays[then.day_of_week],
      then.day_of_month, kMonths[then.month - 1], then.year, then.hour,
      then.minute, then.second));
}
