// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

// The intent filter matching condition types.
enum class ConditionType {
  kScheme,    // Matches the URL scheme (e.g. https, tel).
  kHost,      // Matches the URL host (e.g. www.google.com).
  kPattern,   // Matches the URL pattern (e.g. /abc/*).
  kAction,    // Matches the action type (e.g. view, send).
  kMimeType,  // Matches the top-level mime type (e.g. text/plain).
  kFile,      // Matches against all files.
};

// The pattern match type for intent filter pattern condition.
enum class PatternMatchType {
  kNone = 0,
  kLiteral,
  kPrefix,
  kGlob,
  kMimeType,
  kFileExtension,
  kIsDirectory,
  kSuffix
};

// For pattern type of condition, the value match will be based on the pattern
// match type. If the match_type is kNone, then an exact match with the value
// will be required.
struct COMPONENT_EXPORT(APP_TYPES) ConditionValue {
  ConditionValue(const std::string& value, PatternMatchType match_type);
  ConditionValue(const ConditionValue&) = delete;
  ConditionValue& operator=(const ConditionValue&) = delete;
  ~ConditionValue();

  bool operator==(const ConditionValue& other) const;
  bool operator!=(const ConditionValue& other) const;

  std::string value;
  PatternMatchType match_type;  // This will be None for non pattern conditions.
};

using ConditionValuePtr = std::unique_ptr<ConditionValue>;
using ConditionValues = std::vector<ConditionValuePtr>;

// The condition for an intent filter. It matches if the intent contains this
// condition type and the corresponding value matches with any of the
// condition_values.
struct COMPONENT_EXPORT(APP_TYPES) Condition {
  Condition(ConditionType condition_type, ConditionValues condition_values);
  Condition(const Condition&) = delete;
  Condition& operator=(const Condition&) = delete;
  ~Condition();

  bool operator==(const Condition& other) const;
  bool operator!=(const Condition& other) const;

  std::unique_ptr<Condition> Clone() const;

  ConditionType condition_type;
  ConditionValues condition_values;
};

using ConditionPtr = std::unique_ptr<Condition>;
using Conditions = std::vector<ConditionPtr>;

// An intent filter is defined by an app, and contains a list of conditions that
// an intent needs to match. If all conditions match, then this intent filter
// matches against an intent.
struct COMPONENT_EXPORT(APP_TYPES) IntentFilter {
  IntentFilter();
  IntentFilter(const IntentFilter&) = delete;
  IntentFilter& operator=(const IntentFilter&) = delete;
  ~IntentFilter();

  bool operator==(const IntentFilter& other) const;
  bool operator!=(const IntentFilter& other) const;

  std::unique_ptr<IntentFilter> Clone() const;

  Conditions conditions;

  // Activity which registered this filter. We only fill this field for ARC
  // share intent filters and Web App file_handlers.
  absl::optional<std::string> activity_name;

  // The label shown to the user for this activity.
  absl::optional<std::string> activity_label;
};

using IntentFilterPtr = std::unique_ptr<IntentFilter>;
using IntentFilters = std::vector<IntentFilterPtr>;

// Creates a deep copy of `intent_filters`.
COMPONENT_EXPORT(APP_TYPES)
IntentFilters CloneIntentFilters(const IntentFilters& intent_filters);

COMPONENT_EXPORT(APP_TYPES)
bool IsEqual(const IntentFilters& source, const IntentFilters& target);

// TODO(crbug.com/1253250): Remove these functions after migrating to non-mojo
// AppService.
COMPONENT_EXPORT(APP_TYPES)
ConditionType ConvertMojomConditionTypeToConditionType(
    const apps::mojom::ConditionType& mojom_condition_type);

COMPONENT_EXPORT(APP_TYPES)
apps::mojom::ConditionType ConvertConditionTypeToMojomConditionType(
    const ConditionType& condition_type);

COMPONENT_EXPORT(APP_TYPES)
PatternMatchType ConvertMojomPatternMatchTypeToPatternMatchType(
    const apps::mojom::PatternMatchType& mojom_pattern_match_type);

COMPONENT_EXPORT(APP_TYPES)
apps::mojom::PatternMatchType ConvertPatternMatchTypeToMojomPatternMatchType(
    const PatternMatchType& pattern_match_type);

COMPONENT_EXPORT(APP_TYPES)
ConditionValuePtr ConvertMojomConditionValueToConditionValue(
    const apps::mojom::ConditionValuePtr& mojom_condition_value);

COMPONENT_EXPORT(APP_TYPES)
apps::mojom::ConditionValuePtr ConvertConditionValueToMojomConditionValue(
    const ConditionValuePtr& condition_value);

COMPONENT_EXPORT(APP_TYPES)
ConditionPtr ConvertMojomConditionToCondition(
    const apps::mojom::ConditionPtr& mojom_condition);

COMPONENT_EXPORT(APP_TYPES)
apps::mojom::ConditionPtr ConvertConditionToMojomCondition(
    const ConditionPtr& condition);

COMPONENT_EXPORT(APP_TYPES)
IntentFilterPtr ConvertMojomIntentFilterToIntentFilter(
    const apps::mojom::IntentFilterPtr& mojom_intent_filter);

COMPONENT_EXPORT(APP_TYPES)
apps::mojom::IntentFilterPtr ConvertIntentFilterToMojomIntentFilter(
    const IntentFilterPtr& intent_filter);

COMPONENT_EXPORT(APP_TYPES)
base::flat_map<std::string, std::vector<apps::mojom::IntentFilterPtr>>
ConvertIntentFiltersToMojomIntentFilters(
    const base::flat_map<std::string, apps::IntentFilters>& intent_filter);

COMPONENT_EXPORT(APP_TYPES)
base::flat_map<std::string, apps::IntentFilters>
ConvertMojomIntentFiltersToIntentFilters(
    const base::flat_map<std::string,
                         std::vector<apps::mojom::IntentFilterPtr>>&
        mojom_intent_filter);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_H_
