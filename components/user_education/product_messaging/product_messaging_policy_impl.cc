// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/product_messaging/product_messaging_policy_impl.h"

#include <algorithm>
#include <iterator>

#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "components/user_education/product_messaging/product_messaging_types.h"

namespace user_education {

ProductMessagingPolicyImpl::ProductMessagingPolicyImpl() = default;
ProductMessagingPolicyImpl::~ProductMessagingPolicyImpl() = default;

// static
std::unique_ptr<ProductMessagingPolicyImpl>
ProductMessagingPolicyImpl::CreateDefault() {
  auto result = base::WrapUnique(new ProductMessagingPolicyImpl);
  result->SetSelfBlocking(ProductMessageType::kAnchoredMessage, false);
  result->SetSelfBlocking(ProductMessageType::kLowPriorityIph, false);
  result->SetSelfBlocking(ProductMessageType::kHighPriorityIph, false);
  result->SetEquivalent(ProductMessageType::kHighPriorityIph,
                        ProductMessageType::kLegalOrComplianceNotice);
  return result;
}

bool ProductMessagingPolicyImpl::BlockedByAnyOf(ProductMessageKey key,
                                                const Ids& others,
                                                bool include_self) const {
  if (include_self && others.contains(key.id())) {
    const bool* const explicit_value =
        base::FindOrNull(self_blocking_, key.type());
    if (!explicit_value || *explicit_value) {
      return true;
    }
  }
  if (auto* blocked_by = base::FindOrNull(blocked_by_, key)) {
    for (auto& id : others) {
      if (blocked_by->contains(id)) {
        return true;
      }
    }
  }
  return false;
}

ProductMessagingPolicyImpl::TypeRelationship
ProductMessagingPolicyImpl::GetRelationship(
    ProductMessageType type,
    ProductMessageType other_type) const {
  CHECK_NE(type, ProductMessageType::kNone);
  if (type == other_type) {
    return TypeRelationship::kEquivalentTo;
  }
  if (ignore_and_ignored_by_.contains(type)) {
    return TypeRelationship::kIndependentOf;
  }
  if (auto* const ignored_by =
          base::FindOrNull(ignore_and_ignored_by_, other_type);
      ignored_by && ignored_by->contains(type)) {
    return TypeRelationship::kIndependentOf;
  }
  if (equivalents_.contains({type, other_type})) {
    return TypeRelationship::kEquivalentTo;
  }
  return type < other_type ? TypeRelationship::kSupersededBy
                           : TypeRelationship::kIndependentOf;
}

bool ProductMessagingPolicyImpl::MustShowAfterAnyOf(ProductMessageKey key,
                                                    const Ids& others) const {
  if (const auto* show_after = base::FindOrNull(show_after_, key)) {
    for (auto& id : others) {
      if (show_after->contains(id)) {
        return true;
      }
    }
  }
  return false;
}

void ProductMessagingPolicyImpl::SetSelfBlocking(ProductMessageType type,
                                                 bool blocks_self) {
  self_blocking_[type] = blocks_self;
}

void ProductMessagingPolicyImpl::SetIgnoreAll(
    ProductMessageType type,
    std::initializer_list<ProductMessageType> also_ignored_by) {
  ignore_and_ignored_by_.emplace(type, also_ignored_by);
}

void ProductMessagingPolicyImpl::SetEquivalent(ProductMessageType type1,
                                               ProductMessageType type2) {
  equivalents_.insert({type1, type2});
  equivalents_.insert({type2, type1});
}

void ProductMessagingPolicyImpl::SetBlockedBy(
    ProductMessageKey key,
    std::initializer_list<ProductMessageKey> blocked_by_these) {
  Ids ids;
  std::ranges::transform(blocked_by_these, std::inserter(ids, ids.begin()),
                         &ProductMessageKey::id);
  blocked_by_.emplace(key, std::move(ids));
}

void ProductMessagingPolicyImpl::SetShowAfter(
    ProductMessageKey key,
    std::initializer_list<ProductMessageKey> show_after_these) {
  Ids ids;
  std::ranges::transform(show_after_these, std::inserter(ids, ids.begin()),
                         &ProductMessageKey::id);
  show_after_.emplace(key, std::move(ids));
}

}  // namespace user_education
