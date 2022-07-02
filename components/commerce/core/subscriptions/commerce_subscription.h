// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_COMMERCE_SUBSCRIPTION_H_
#define COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_COMMERCE_SUBSCRIPTION_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace commerce {

// The type of subscription.
enum class SubscriptionType {
  // Unspecified type.
  kTypeUnspecified = 0,
  // Subscription for price tracking.
  kPriceTrack = 1,
};

// The type of subscription identifier.
enum class IdentifierType {
  // Unspecified identifier type.
  kIdentifierTypeUnspecified = 0,
  // Offer id identifier, used in chrome-managed price tracking.
  kOfferId = 1,
  // Product cluster id identifier, used in user-managed price tracking.
  kProductClusterId = 2,
};

// The type of subscription management.
enum class ManagementType {
  // Unspecified management type.
  kTypeUnspecified = 0,
  // Automatic chrome-managed subscription.
  kChromeManaged = 1,
  // Explicit user-managed subscription.
  kUserManaged = 2,
};

struct UserSeenOffer {
  UserSeenOffer(uint64_t offer_id,
                long user_seen_price,
                const std::string& country_code);
  ~UserSeenOffer();

  const uint64_t offer_id;
  const long user_seen_price;
  const std::string country_code;
};

struct CommerceSubscription {
  CommerceSubscription(SubscriptionType type,
                       IdentifierType id_type,
                       uint64_t id,
                       ManagementType management_type);
  CommerceSubscription(SubscriptionType type,
                       IdentifierType id_type,
                       uint64_t id,
                       ManagementType management_type,
                       absl::optional<UserSeenOffer> user_seen_offer);
  ~CommerceSubscription();

  const SubscriptionType type;
  const IdentifierType id_type;
  const uint64_t id;
  const ManagementType management_type;
  const absl::optional<UserSeenOffer> user_seen_offer;
  const uint64_t timestamp;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_COMMERCE_SUBSCRIPTION_H_
