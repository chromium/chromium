// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/suggestion/filter_suggestion_message_util.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/i18n/time_formatting.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace multistep_filter {

namespace {

struct DateComponents {
  int day;
  int month;
  int year;
  std::u16string day_str;
  std::u16string month_str;
  std::u16string year_str;
};

DateComponents GetDateComponents(base::Time time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  return {
      .day = exploded.day_of_month,
      .month = exploded.month,
      .year = exploded.year,
      .day_str = base::LocalizedTimeFormatWithPattern(time, "d"),
      .month_str = base::LocalizedTimeFormatWithPattern(time, "MMM"),
      .year_str = base::NumberToString16(exploded.year),
  };
}

bool IsFullYearOrMore(const DateComponents& start, const DateComponents& end) {
  if (end.year - start.year > 1) {
    return true;
  }
  if (end.year - start.year == 1) {
    if (end.month > start.month) {
      return true;
    }
    if (end.month == start.month) {
      return end.day >= start.day;
    }
  }
  return false;
}

// Parses a date string in ISO 8601 format ("YYYY-MM-DD").
std::optional<base::Time> ParseDateFromISO8601(std::u16string_view date) {
  base::Time time;
  std::string date_utf8 = base::UTF16ToUTF8(date);
  if (base::Time::FromString(date_utf8.c_str(), &time)) {
    return time;
  }
  return std::nullopt;
}

// TODO(crbug.com/515832188): Improve date range formatting to use localized
// patterns rather than assume the hardcoded Month + Date ordering and " - "
// separator.
std::u16string FormatDateRange(std::u16string_view start,
                               std::u16string_view end) {
  if (start.empty()) {
    return u"";
  }

  std::optional<base::Time> start_date = ParseDateFromISO8601(start);
  if (!start_date) {
    return u"";
  }

  DateComponents start_comp = GetDateComponents(*start_date);

  if (end.empty()) {
    return base::StrCat({start_comp.month_str, u" ", start_comp.day_str});
  }

  std::optional<base::Time> end_date = ParseDateFromISO8601(end);
  if (!end_date) {
    return u"";
  }

  DateComponents end_comp = GetDateComponents(*end_date);

  bool show_years = IsFullYearOrMore(start_comp, end_comp);

  if (show_years) {
    return base::StrCat({start_comp.month_str, u" ", start_comp.day_str, u", ",
                         start_comp.year_str, internal::kDateRangeSeparator,
                         end_comp.month_str, u" ", end_comp.day_str, u", ",
                         end_comp.year_str});
  }

  // Same year and same month.
  if (start_comp.year == end_comp.year && start_comp.month == end_comp.month) {
    if (start_comp.day == end_comp.day) {
      return base::StrCat({start_comp.month_str, u" ", start_comp.day_str});
    }
    return base::StrCat({start_comp.month_str, u" ", start_comp.day_str,
                         internal::kDateRangeSeparator, end_comp.day_str});
  }

  // Different months but within a span of 1 year.
  return base::StrCat({start_comp.month_str, u" ", start_comp.day_str,
                       internal::kDateRangeSeparator, end_comp.month_str, u" ",
                       end_comp.day_str});
}

std::optional<std::u16string_view> GetAttributeValue(
    const std::vector<FilterAttributeUiLabel>& attribute_ui_labels,
    std::string_view key) {
  if (auto it = std::ranges::find(attribute_ui_labels, key,
                                  &FilterAttributeUiLabel::key);
      it != attribute_ui_labels.end()) {
    return it->attribute_value;
  }
  return std::nullopt;
}

// Replaces placeholders in `template_str` (formatted as "{KEY}") with the
// actual string values found in `attribute_ui_labels`. If a key is not found
// in the labels, the placeholder is left intact in the returned string.
std::u16string SubstitutePlaceholders(
    std::u16string_view template_str,
    const std::vector<FilterAttributeUiLabel>& attribute_ui_labels) {
  std::u16string result;
  result.reserve(template_str.size());

  // Scan the string char by char, looking for matching braces that enclose
  // keys.
  for (size_t start = 0; start < template_str.size(); ++start) {
    if (template_str[start] != u'{') {
      result.push_back(template_str[start]);
      continue;
    }

    const size_t end = template_str.find(u'}', start);
    if (end == std::u16string_view::npos) {
      result.push_back(template_str[start]);
      continue;
    }

    std::u16string_view key_u16 =
        template_str.substr(start + 1, end - start - 1);
    std::string key = base::UTF16ToUTF8(key_u16);

    // Find the replacement for this key.
    if (auto it = std::ranges::find(attribute_ui_labels, key,
                                    &FilterAttributeUiLabel::key);
        it != attribute_ui_labels.end()) {
      result.append(it->attribute_value);
      start = end;
      continue;
    }

    result.push_back(template_str[start]);
  }
  return result;
}
// Resolves the detail value for `key`.
// Applies plural rules if present in the config, skips zero values by returning
// nullopt, and falls back to the raw value otherwise.
std::optional<std::u16string> ResolveDetailValue(
    std::string_view key,
    const base::DictValue* task_dict,
    const std::vector<FilterAttributeUiLabel>& attribute_ui_labels) {
  // TODO(crbug.com/515833124): Move away from rigid "one" vs "other" JSON
  // processing to localized string resources or ICU to support
  // multi-variant plural bucket languages.
  std::optional<std::u16string_view> attribute_value =
      GetAttributeValue(attribute_ui_labels, key);
  if (!attribute_value) {
    return std::nullopt;
  }

  int count = 0;
  if (base::StringToInt(std::u16string(*attribute_value), &count) &&
      count == 0) {
    return std::nullopt;  // Skip zero!
  }

  const base::DictValue* plurals_dict =
      task_dict->FindDict(internal::kPluralsKey);
  const base::DictValue* plural_rules =
      plurals_dict ? plurals_dict->FindDict(key) : nullptr;
  const std::string* plural_template =
      plural_rules ? plural_rules->FindString(count == 1 ? internal::kOneKey
                                                         : internal::kOtherKey)
                   : nullptr;

  if (!plural_template) {
    // Fallback to raw value if no plural rule
    return std::u16string(*attribute_value);
  }

  // TODO(b/517432894): Consider caching the converted UTF-16 templates to avoid
  // repetitive conversion overhead if this becomes a performance bottleneck in
  // filter_suggestion_message_util.cc.
  std::u16string resolved_value = base::UTF8ToUTF16(*plural_template);
  base::ReplaceSubstringsAfterOffset(
      &resolved_value, 0, internal::kValuePlaceholder, *attribute_value);
  return resolved_value;
}

// Generates a formatted date range string (e.g., "Jun 10 - 15") by looking up
// the start and end date attributes specified by `kDateKeysKey` in the config.
std::u16string GetDateRangeMessage(
    const base::DictValue* task_dict,
    const std::vector<FilterAttributeUiLabel>& attribute_ui_labels) {
  const base::DictValue* date_keys =
      task_dict->FindDict(internal::kDateKeysKey);
  if (!date_keys) {
    return u"";
  }

  const std::string* start_key = date_keys->FindString(internal::kStartKey);
  if (!start_key) {
    return u"";
  }

  std::optional<std::u16string_view> start_date =
      GetAttributeValue(attribute_ui_labels, *start_key);
  if (!start_date) {
    return u"";
  }

  const std::string* end_key = date_keys->FindString(internal::kEndKey);
  std::optional<std::u16string_view> end_date =
      end_key ? GetAttributeValue(attribute_ui_labels, *end_key) : std::nullopt;

  return FormatDateRange(*start_date, end_date.value_or(u""));
}

// Returns a list of formatted detail strings based on the order specified in
// `kDetailsOrderKey` and applying pluralization rules defined in `kPluralsKey`.
std::vector<std::u16string> GetDetailsList(
    const base::DictValue* task_dict,
    const std::vector<FilterAttributeUiLabel>& attribute_ui_labels) {
  std::vector<std::u16string> details;
  const base::ListValue* details_order =
      task_dict->FindList(internal::kDetailsOrderKey);
  if (!details_order) {
    return details;
  }
  for (const base::Value& item : *details_order) {
    if (!item.is_string()) {
      continue;
    }
    std::string key = item.GetString();

    if (key == internal::kDateStringKey) {
      std::u16string date_str =
          GetDateRangeMessage(task_dict, attribute_ui_labels);
      if (!date_str.empty()) {
        details.emplace_back(std::move(date_str));
      }
      continue;
    }

    if (std::optional<std::u16string> detail_message =
            ResolveDetailValue(key, task_dict, attribute_ui_labels)) {
      details.emplace_back(std::move(*detail_message));
    }
  }
  return details;
}

}  // namespace

std::optional<std::u16string> GenerateMessageWithConfig(
    const std::string& task_type,
    const std::vector<FilterAttributeUiLabel>& attribute_ui_labels,
    const base::DictValue& config) {
  // Look up the dictionary for the specific task_type
  const base::DictValue* task_dict = config.FindDict(task_type);
  if (!task_dict) {
    return std::nullopt;
  }

  const std::string* template_str_ptr =
      task_dict->FindString(internal::kTemplateKey);
  if (!template_str_ptr) {
    return std::nullopt;
  }

  std::u16string message_template = base::UTF8ToUTF16(*template_str_ptr);
  std::u16string suggestion_message =
      SubstitutePlaceholders(message_template, attribute_ui_labels);
  std::vector<std::u16string> details =
      GetDetailsList(task_dict, attribute_ui_labels);

  if (!details.empty()) {
    suggestion_message =
        base::StrCat({suggestion_message, internal::kTemplateDetailsSeparator,
                      base::JoinString(details, internal::kDetailsSeparator)});
  }

  return suggestion_message;
}

}  // namespace multistep_filter
