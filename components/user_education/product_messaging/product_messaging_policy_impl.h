// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_PRODUCT_MESSAGING_PRODUCT_MESSAGING_POLICY_IMPL_H_
#define COMPONENTS_USER_EDUCATION_PRODUCT_MESSAGING_PRODUCT_MESSAGING_POLICY_IMPL_H_

#include <initializer_list>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "components/user_education/product_messaging/product_messaging_policy.h"
#include "components/user_education/product_messaging/product_messaging_types.h"

namespace user_education {

class ProductMessagingPolicyImpl : public ProductMessagingPolicy {
 public:
  ~ProductMessagingPolicyImpl() override;

  // Creates a default instance of the policy with some basic values set.
  static std::unique_ptr<ProductMessagingPolicyImpl> CreateDefault();

  // ProductMessagingPolicy:
  bool BlockedByAnyOf(ProductMessageKey key,
                      const Ids& others,
                      bool include_self) const override;
  TypeRelationship GetRelationship(
      ProductMessageType type,
      ProductMessageType other_type) const override;
  bool MustShowAfterAnyOf(ProductMessageKey key,
                          const Ids& others) const override;

  // Indicate whether `type` should be self-blocking; that is, whether having
  // successfully shown a notice should prevent that notice from showing again
  // in the same session. For systems which re-use a single key for different
  // messages, set this to false. Default is true.
  void SetSelfBlocking(ProductMessageType type, bool blocks_self);

  // Indicates that `type` can be shown irrespective of what other notices are
  // showing. This type may still block other messages (except for those in
  // `also_ignored_by`).
  void SetIgnoreAll(ProductMessageType type,
                    ProductMessageTypeSet also_ignored_by = {});

  // Set that `type1` and `type2` are equivalent (i.e. active notices will block
  // each other, but without any precedence).
  void SetEquivalent(ProductMessageType type1, ProductMessageType type2);

  // Set that `key` cannot be shown if any of `blocked_by_these` have been shown
  // this session.
  void SetBlockedBy(ProductMessageKey key,
                    std::initializer_list<ProductMessageKey> blocked_by_these);

  // Set that `key` cannot be shown if any of `show_after_these` are queued.
  void SetShowAfter(ProductMessageKey key,
                    std::initializer_list<ProductMessageKey> show_after_these);

 protected:
  ProductMessagingPolicyImpl();

 private:
  std::map<ProductMessageType, bool> self_blocking_;
  std::map<ProductMessageKey, Ids> blocked_by_;
  std::map<ProductMessageKey, Ids> show_after_;
  std::set<std::pair<ProductMessageType, ProductMessageType>> equivalents_;
  std::map<ProductMessageType, ProductMessageTypeSet> ignore_and_ignored_by_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_PRODUCT_MESSAGING_PRODUCT_MESSAGING_POLICY_IMPL_H_
