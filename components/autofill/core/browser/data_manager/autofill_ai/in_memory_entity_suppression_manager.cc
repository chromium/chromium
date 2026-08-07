// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/in_memory_entity_suppression_manager.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"

namespace autofill {

namespace {

// Delimiter separating entity type and attribute values in canonical strings.
constexpr std::string_view kSeparator = "-";

// Formats a canonical string for a satisfied merge constraint set.
std::string GetCanonicalString(const EntityInstance& entity,
                               const DenseSet<AttributeType>& constraint_set) {
  std::string canonical(entity.type().name_as_string());
  for (AttributeType attr_type : constraint_set) {
    std::string value =
        base::UTF16ToUTF8(entity.attribute(attr_type)->GetCompleteRawInfo());
    base::StrAppend(&canonical, {kSeparator, attr_type.name_as_string(),
                                 kSeparator, value});
  }
  return canonical;
}

}  // namespace

InMemoryEntitySuppressionManager::InMemoryEntitySuppressionManager() = default;

InMemoryEntitySuppressionManager::~InMemoryEntitySuppressionManager() = default;

std::vector<std::string> InMemoryEntitySuppressionManager::GetCanonicalStrings(
    const EntityInstance& entity) const {
  auto is_constraint_satisfied =
      [&](const DenseSet<AttributeType>& constraint_set) {
        return std::ranges::all_of(
            constraint_set, [&](AttributeType attr_type) {
              return entity.attribute(attr_type).has_value();
            });
      };

  std::vector<std::string> canonical_strings;
  for (const DenseSet<AttributeType>& constraint_set :
       entity.type().merge_constraints()) {
    if (is_constraint_satisfied(constraint_set)) {
      canonical_strings.push_back(GetCanonicalString(entity, constraint_set));
    }
  }
  return canonical_strings;
}

bool InMemoryEntitySuppressionManager::SuppressEntity(
    const EntityInstance& entity) {
  std::vector<std::string> canonical_strings = GetCanonicalStrings(entity);
  size_t original_size = suppressed_keys_.size();
  suppressed_keys_.insert(std::make_move_iterator(canonical_strings.begin()),
                          std::make_move_iterator(canonical_strings.end()));
  return suppressed_keys_.size() > original_size;
}

bool InMemoryEntitySuppressionManager::UnsuppressEntity(
    const EntityInstance& entity) {
  std::vector<std::string> canonical_strings = GetCanonicalStrings(entity);
  size_t original_size = suppressed_keys_.size();
  for (const std::string& canonical : canonical_strings) {
    suppressed_keys_.erase(canonical);
  }
  return suppressed_keys_.size() < original_size;
}

bool InMemoryEntitySuppressionManager::IsSuppressed(
    const EntityInstance& entity) const {
  std::vector<std::string> canonical_strings = GetCanonicalStrings(entity);
  return std::ranges::any_of(canonical_strings,
                             [this](const std::string& canonical) {
                               return suppressed_keys_.contains(canonical);
                             });
}

}  // namespace autofill
