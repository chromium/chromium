// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/intent_helper/intent_filter.h"

#include <algorithm>
#include <utility>

#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/experiences/arc/intent_helper/intent_constants.h"
#include "url/gurl.h"

namespace arc {

bool IsKnownPatternType(PatternType type) {
  switch (type) {
    case PatternType::kUnknown:
    case PatternType::kLiteral:
    case PatternType::kPrefix:
    case PatternType::kSimpleGlob:
    case PatternType::kAdvancedGlob:
    case PatternType::kSuffix:
      return true;
  }
  return false;
}

std::ostream& operator<<(std::ostream& os, PatternType type) {
  switch (type) {
    case PatternType::kUnknown:
      return os << "kUnknown";
    case PatternType::kLiteral:
      return os << "kLiteral";
    case PatternType::kPrefix:
      return os << "kPrefix";
    case PatternType::kSimpleGlob:
      return os << "kSimpleGlob";
    case PatternType::kAdvancedGlob:
      return os << "kAdvancedGlob";
    case PatternType::kSuffix:
      return os << "kSuffix";
  }
  return os << "Unknown PatternType value: " << static_cast<int>(type);
}

IntentFilter::IntentFilter() = default;
IntentFilter::IntentFilter(IntentFilter&& other) = default;

IntentFilter::IntentFilter(
    const std::string& package_name,
    std::vector<std::string> actions,
    std::vector<IntentFilter::AuthorityEntry> authorities,
    std::vector<IntentFilter::PatternMatcher> paths,
    std::vector<std::string> schemes,
    std::vector<std::string> mime_types)
    : package_name_(package_name),
      actions_(std::move(actions)),
      authorities_(std::move(authorities)),
      schemes_(std::move(schemes)),
      mime_types_(std::move(mime_types)) {
  // In order to register a path we need to have at least one authority.
  if (!authorities_.empty()) {
    paths_ = std::move(paths);
  }
}

IntentFilter::IntentFilter(
    const std::string& package_name,
    const std::string& activity_name,
    const std::string& activity_label,
    std::vector<std::string> actions,
    std::vector<IntentFilter::AuthorityEntry> authorities,
    std::vector<IntentFilter::PatternMatcher> paths,
    std::vector<std::string> schemes,
    std::vector<std::string> mime_types)
    : package_name_(package_name),
      activity_name_(activity_name),
      activity_label_(activity_label),
      actions_(std::move(actions)),
      authorities_(std::move(authorities)),
      schemes_(std::move(schemes)),
      mime_types_(std::move(mime_types)) {
  // In order to register a path we need to have at least one authority.
  if (!authorities_.empty()) {
    paths_ = std::move(paths);
  }
}

IntentFilter::~IntentFilter() = default;

IntentFilter& IntentFilter::operator=(IntentFilter&& other) = default;

IntentFilter::AuthorityEntry::AuthorityEntry() = default;
IntentFilter::AuthorityEntry::AuthorityEntry(
    IntentFilter::AuthorityEntry&& other) = default;

IntentFilter::AuthorityEntry& IntentFilter::AuthorityEntry::operator=(
    IntentFilter::AuthorityEntry&& other) = default;

IntentFilter::AuthorityEntry::AuthorityEntry(const std::string& host, int port)
    : host_(host), port_(port) {
  // Wildcards are only allowed at the front of the host string.
  wild_ = !host_.empty() && host_[0] == '*';
  if (wild_) {
    host_ = host_.substr(1);
  }

  // TODO(kenobi): Not i18n-friendly.  Figure out how to correctly deal with
  // IDNs.
  host_ = base::ToLowerASCII(host_);
}

IntentFilter::PatternMatcher::PatternMatcher() = default;
IntentFilter::PatternMatcher::PatternMatcher(
    IntentFilter::PatternMatcher&& other) = default;

IntentFilter::PatternMatcher::PatternMatcher(const std::string& pattern,
                                             PatternType match_type)
    : pattern_(pattern), match_type_(match_type) {}

IntentFilter::PatternMatcher& IntentFilter::PatternMatcher::operator=(
    IntentFilter::PatternMatcher&& other) = default;

}  // namespace arc
