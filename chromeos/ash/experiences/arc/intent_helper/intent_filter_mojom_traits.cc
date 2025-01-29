// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/intent_helper/intent_filter_mojom_traits.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"

namespace mojo {

bool StructTraits<arc::mojom::IntentFilterDataView, arc::IntentFilter>::Read(
    arc::mojom::IntentFilterDataView data,
    arc::IntentFilter* out) {
  std::vector<arc::IntentFilter::AuthorityEntry> authorities;
  if (!data.ReadDataAuthorities(&authorities)) {
    return false;
  }

  std::vector<arc::IntentFilter::PatternMatcher> paths;
  if (!data.ReadDataPaths(&paths)) {
    return false;
  }

  std::optional<std::string> package_name;
  if (!data.ReadPackageName(&package_name)) {
    return false;
  }

  std::vector<std::string> schemes;
  if (!data.ReadDataSchemes(&schemes)) {
    return false;
  }

  std::vector<std::string> actions;
  if (!data.ReadActions(&actions)) {
    return false;
  }

  std::vector<std::string> mime_types;
  if (!data.ReadMimeTypes(&mime_types)) {
    return false;
  }

  std::optional<std::string> activity_name;
  if (!data.ReadActivityName(&activity_name)) {
    return false;
  }

  std::optional<std::string> activity_label;
  if (!data.ReadActivityLabel(&activity_label)) {
    return false;
  }

  *out = arc::IntentFilter(std::move(package_name).value_or(std::string()),
                           std::move(activity_name).value_or(std::string()),
                           std::move(activity_label).value_or(std::string()),
                           std::move(actions), std::move(authorities),
                           std::move(paths), std::move(schemes),
                           std::move(mime_types));
  return true;
}

bool StructTraits<arc::mojom::AuthorityEntryDataView,
                  arc::IntentFilter::AuthorityEntry>::
    Read(arc::mojom::AuthorityEntryDataView data,
         arc::IntentFilter::AuthorityEntry* out) {
  std::string host;
  if (!data.ReadHost(&host)) {
    return false;
  }

  *out = arc::IntentFilter::AuthorityEntry(std::move(host), data.port());
  return true;
}

arc::mojom::PatternType
EnumTraits<arc::mojom::PatternType, arc::PatternType>::ToMojom(
    arc::PatternType input) {
  switch (input) {
    case arc::PatternType::kUnknown:
      return arc::mojom::PatternType::kUnknown;
    case arc::PatternType::kLiteral:
      return arc::mojom::PatternType::PATTERN_LITERAL;
    case arc::PatternType::kPrefix:
      return arc::mojom::PatternType::PATTERN_PREFIX;
    case arc::PatternType::kSimpleGlob:
      return arc::mojom::PatternType::PATTERN_SIMPLE_GLOB;
    case arc::PatternType::kAdvancedGlob:
      return arc::mojom::PatternType::PATTERN_ADVANCED_GLOB;
    case arc::PatternType::kSuffix:
      return arc::mojom::PatternType::PATTERN_SUFFIX;
  }

  NOTREACHED();
}

bool EnumTraits<arc::mojom::PatternType, arc::PatternType>::FromMojom(
    arc::mojom::PatternType input,
    arc::PatternType* output) {
  switch (input) {
    case arc::mojom::PatternType::kUnknown:
      *output = arc::PatternType::kUnknown;
      return true;
    case arc::mojom::PatternType::PATTERN_LITERAL:
      *output = arc::PatternType::kLiteral;
      return true;
    case arc::mojom::PatternType::PATTERN_PREFIX:
      *output = arc::PatternType::kPrefix;
      return true;
    case arc::mojom::PatternType::PATTERN_SIMPLE_GLOB:
      *output = arc::PatternType::kSimpleGlob;
      return true;
    case arc::mojom::PatternType::PATTERN_ADVANCED_GLOB:
      *output = arc::PatternType::kAdvancedGlob;
      return true;
    case arc::mojom::PatternType::PATTERN_SUFFIX:
      *output = arc::PatternType::kSuffix;
      return true;
  }

  return false;
}

bool StructTraits<arc::mojom::PatternMatcherDataView,
                  arc::IntentFilter::PatternMatcher>::
    Read(arc::mojom::PatternMatcherDataView data,
         arc::IntentFilter::PatternMatcher* out) {
  std::string pattern;
  if (!data.ReadPattern(&pattern)) {
    return false;
  }

  arc::PatternType type;
  if (!data.ReadType(&type)) {
    return false;
  }

  *out = arc::IntentFilter::PatternMatcher(std::move(pattern), type);
  return true;
}

}  // namespace mojo
