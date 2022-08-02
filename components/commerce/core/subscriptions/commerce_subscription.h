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
  UserSeenOffer(std::string offer_id,
                long user_seen_price,
                std::string country_code);
  UserSeenOffer(const UserSeenOffer&);
  UserSeenOffer& operator=(const UserSeenOffer&);
  ~UserSeenOffer();

  std::string offer_id;
  long user_seen_price;
  std::string country_code;
};

extern const int64_t kUnknownSubscriptionTimestamp;

struct CommerceSubscription {
  CommerceSubscription(
      SubscriptionType type,
      IdentifierType id_type,
      std::string id,
      ManagementType management_type,
      int64_t timestamp = kUnknownSubscriptionTimestamp,
      absl::optional<UserSeenOffer> user_seen_offer = absl::nullopt);
  CommerceSubscription(const CommerceSubscription&);
  CommerceSubscription& operator=(const CommerceSubscription&);
  ~CommerceSubscription();

  SubscriptionType type;
  IdentifierType id_type;
  std::string id;
  ManagementType management_type;
  int64_t timestamp;
  absl::optional<UserSeenOffer> user_seen_offer;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_COMMERCE_SUBSCRIPTION_H_
