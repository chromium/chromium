// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/style_rule_matcher.h"

#include <algorithm>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "components/subresource_filter/core/common/style_rule_indexer.h"

namespace subresource_filter {

namespace {
constexpr char kClassPrefix = '.';
constexpr char kIdPrefix = '#';

// Removes the '.' or '#' prefix from a class or ID name, respectively, if
// present.
std::string_view StripPrefix(std::string_view name) {
  if (!name.empty() && (name[0] == kClassPrefix || name[0] == kIdPrefix)) {
    return name.substr(1);
  }
  return name;
}

// Finds all selectors in `rules` that match the given `name` (after stripping
// prefixes).
const flat::NameToSelectors* FindEntry(
    const flatbuffers::Vector<flatbuffers::Offset<flat::NameToSelectors>>* map,
    std::string_view name) {
  auto it = std::ranges::lower_bound(
      *map, name, {}, [](const flat::NameToSelectors* entry) {
        return StripPrefix(entry->name()->string_view());
      });
  if (it != map->end() && StripPrefix((*it)->name()->string_view()) == name) {
    return *it;
  }
  return nullptr;
}

// Finds all selectors in `rules` that match the given `name` (after stripping
// prefixes) and adds them to `out_selectors`, provided they are not in
// `excluded_indices`.
void GetSelectorsForName(const flatbuffers::Vector<
                             flatbuffers::Offset<flat::NameToSelectors>>* rules,
                         const flat::StyleRuleIndex* index,
                         std::string_view name,
                         const base::flat_set<uint16_t>& excluded_indices,
                         std::vector<std::string_view>& out_selectors) {
  const flat::NameToSelectors* entry = FindEntry(rules, name);
  if (!entry) {
    return;
  }

  auto add_explicit_rule = [&](uint16_t rule_index) {
    if (!excluded_indices.contains(rule_index)) {
      out_selectors.emplace_back(
          index->selectors()->Get(rule_index)->string_view());
    }
  };

  out_selectors.reserve(
      1 + (entry->more_indices() ? entry->more_indices()->size() : 0));
  uint16_t rule_index = entry->first_index();

  if (rule_index == StyleRuleIndexer::kImplicitStyleRuleIndex) {
    // Implicit rules are only supported for class and ID maps, where the
    // name is the selector. They should never be in the domain map.
    DCHECK(rules != index->domain_map());
    out_selectors.emplace_back(entry->name()->string_view());
  } else {
    add_explicit_rule(rule_index);
  }

  if (entry->more_indices()) {
    for (uint16_t more_rule_index : *entry->more_indices()) {
      add_explicit_rule(more_rule_index);
    }
  }
}

// If the document is `a.b.example`, then the rules for `a.b.example` and
// `b.example` apply. This utility function makes it easier to walk the
// subdomains. Note that IP addresses would also do this, which is weird, but ad
// blockers don't filter on domain for style rules.
template <typename Callback>
void ForEachParentDomain(std::string_view host, Callback callback) {
  for (std::string_view sub_host = host; !sub_host.empty();) {
    callback(sub_host);
    size_t dot_pos = sub_host.find('.');
    if (dot_pos == std::string_view::npos) {
      break;
    }
    sub_host = sub_host.substr(dot_pos + 1);
  }
}

}  // namespace

StyleRuleMatcher::StyleRuleMatcher(const flat::StyleRuleIndex* index)
    : index_(index),
      bloom_filter_(index_ && index_->bloom_filter()
                        ? UNSAFE_BUFFERS(base::span<const uint8_t>(
                              index_->bloom_filter()->data(),
                              index_->bloom_filter()->size()))
                        : base::span<const uint8_t>()) {}

StyleRuleMatcher::~StyleRuleMatcher() = default;

bool StyleRuleMatcher::CheckBloomBits(uint32_t hash) const {
  return bloom_filter_.MaybeContains(hash);
}

bool StyleRuleMatcher::MaybeHasStyleRule(uint32_t hash) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!index_) {
    return false;
  }
  return CheckBloomBits(hash);
}

void StyleRuleMatcher::GetDomainSelectors(
    const url::Origin& document_origin,
    std::vector<std::string_view>& out_selectors) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!index_ || !index_->selectors() || !index_->domain_map()) {
    return;
  }

  const auto& excluded_indices = GetExcludedIndices(document_origin);

  // It is safe to use the precursor host for opaque origins here because this
  // is used for identifying and filtering ad content. This allows applying
  // site-specific selectors to opaque frames created by that site.
  std::string_view host =
      document_origin.GetTupleOrPrecursorTupleIfOpaque().host();

  // Get selectors for this host as well as the parent domain without the
  // subdomain.
  ForEachParentDomain(host, [&](std::string_view sub_host) {
    GetSelectorsForName(index_->domain_map(), index_, sub_host,
                        excluded_indices, out_selectors);
  });
}

void StyleRuleMatcher::GetSelectorsForMap(
    const url::Origin& document_origin,
    const flatbuffers::Vector<flatbuffers::Offset<flat::NameToSelectors>>* map,
    std::string_view name,
    uint32_t hash,
    std::vector<std::string_view>& out_selectors) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!index_ || !index_->selectors() || !map) {
    return;
  }
  if (!CheckBloomBits(hash)) {
    return;
  }

  const auto& excluded_indices = GetExcludedIndices(document_origin);
  GetSelectorsForName(map, index_, name, excluded_indices, out_selectors);
}

void StyleRuleMatcher::GetSelectorsByClass(
    const url::Origin& document_origin,
    std::string_view class_name,
    uint32_t hash,
    std::vector<std::string_view>& out_selectors) const {
  GetSelectorsForMap(document_origin, index_ ? index_->class_map() : nullptr,
                     class_name, hash, out_selectors);
}

void StyleRuleMatcher::GetSelectorsById(
    const url::Origin& document_origin,
    std::string_view id_name,
    uint32_t hash,
    std::vector<std::string_view>& out_selectors) const {
  GetSelectorsForMap(document_origin, index_ ? index_->id_map() : nullptr,
                     id_name, hash, out_selectors);
}

const base::flat_set<uint16_t>& StyleRuleMatcher::GetExcludedIndices(
    const url::Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cached_origin_ && *cached_origin_ == origin) {
    return *cached_excluded_indices_;
  }

  cached_origin_ = origin;
  cached_excluded_indices_ = base::flat_set<uint16_t>();

  if (!index_ || !index_->exclusion_map()) {
    return *cached_excluded_indices_;
  }

  // It is safe to use the precursor host for opaque origins here because this
  // is used for identifying and filtering ad content. This allows applying
  // site-specific selectors to opaque frames created by that site.
  std::string_view host = origin.GetTupleOrPrecursorTupleIfOpaque().host();

  const auto* exclusions = index_->exclusion_map();
  std::vector<uint16_t> excluded_vector;

  // Find excluded selectors for this host, as well as the parent domain suffix.
  ForEachParentDomain(host, [&](std::string_view sub_host) {
    const auto* entry = FindEntry(exclusions, sub_host);
    if (entry) {
      if (entry->first_index() != StyleRuleIndexer::kImplicitStyleRuleIndex) {
        excluded_vector.emplace_back(entry->first_index());
      }
      if (entry->more_indices()) {
        for (uint16_t more_rule_index : *entry->more_indices()) {
          excluded_vector.emplace_back(more_rule_index);
        }
      }
    }
  });
  *cached_excluded_indices_ =
      base::flat_set<uint16_t>(std::move(excluded_vector));
  return *cached_excluded_indices_;
}

}  // namespace subresource_filter
