// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_SUGGESTION_FILTER_SUGGESTION_MESSAGE_UTIL_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_SUGGESTION_FILTER_SUGGESTION_MESSAGE_UTIL_H_

#include <optional>
#include <string>
#include <vector>

#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"

namespace base {
class DictValue;
}

namespace multistep_filter {

// Generates the localized message string for the given `suggestion`.
namespace internal {

inline constexpr char kTemplateKey[] = "template";
inline constexpr char kDetailsOrderKey[] = "details_order";
inline constexpr char kPluralsKey[] = "plurals";
inline constexpr char kOneKey[] = "one";
inline constexpr char kOtherKey[] = "other";
inline constexpr char kDateStringKey[] = "DATE_STRING";
inline constexpr char kDateKeysKey[] = "date_keys";
inline constexpr char kStartKey[] = "start";
inline constexpr char kEndKey[] = "end";
// These are fragments of the final user-facing message and hence defined as
// utf16 format to avoid conversions
inline constexpr char16_t kValuePlaceholder[] = u"{VALUE}";
inline constexpr char16_t kTemplateDetailsSeparator[] = u" ";
inline constexpr char16_t kDetailsSeparator[] = u" • ";
inline constexpr char16_t kDateRangeSeparator[] = u" - ";

}  // namespace internal

// Generates a localized message string for the given `task_type` using the
// provided `config` and `attribute_ui_labels`.
//
// Example config structure:
// {
//   "FILTER_TASK": {
//     "template": "Search for {FILTER} ?",
//     "details_order": ["COLOR", "QUANTITY", "DATE_STRING"],
//     "date_keys": {
//       "start": "START_DATE",
//       "end": "END_DATE"
//     },
//     "plurals": {
//       "QUANTITY": {
//         "one": "1 item",
//         "other": "{VALUE} items"
//       }
//     }
//   }
// }
//
// Example input labels:
//   [{"FILTER", u"Books"}, {"COLOR", u"Red"}, {"QUANTITY", u"5"},
//    {"START_DATE", u"2026-06-10"}, {"END_DATE", u"2026-06-15"}]
//
// Example final message output:
//   "Search for Books ? Red • 5 items • Jun 10 - 15"
std::optional<std::u16string> GenerateMessageWithConfig(
    const std::string& task_type,
    const std::vector<FilterAttributeUiLabel>& attribute_ui_labels,
    const base::DictValue& config);

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_SUGGESTION_FILTER_SUGGESTION_MESSAGE_UTIL_H_
