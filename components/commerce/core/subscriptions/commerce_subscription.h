// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_COMMERCE_SUBSCRIPTION_H_
#define COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_COMMERCE_SUBSCRIPTION_H_

#include <stdint.h>

#include <optional>
#include <string>

/**
 * To add a new SubscriptionType / IdentifierType / ManagementType:
 * 1. Define the type in the enum class in commerce_subscription.h.
 * 2. Update the conversion methods between the type and std::string in
 * commerce_subscription.cc.
 * 3. Add the corresponding entry in {@link
 * commerce_subscription_db_content.proto} to ensure the storage works
 * correctly.
 */

namespace commerce {

// The type of subscription.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.commerce.core
enum class SubscriptionType {
  // Unspecified type.
  kTypeUnspecified = 0,
  // Subscription for price tracking.
  kPriceTrack = 1,
};

// The type of subscription identifier.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.commerce.core
enum class IdentifierType {
  // Unspecified identifier type.
  kIdentifierTypeUnspecified = 0,
  // Offer id identifier, used in chrome-managed price tracking.
  kOfferId = 1,
  // Product cluster id identifier, used in user-managed price tracking.
  kProductClusterId = 2,
};

// The type of subscription management.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.commerce.core
enum class ManagementType {
  // Unspecified management type.
  kTypeUnspecified = 0,
  // Automatic chrome-managed subscription.
  kChromeManaged = 1,
  // Explicit user-managed subscription.
  kUserManaged = 2,
};

// The user seen offer data upon price tracking subscribing, used to improve the
// price drop notification quality.
struct UserSeenOffer {
  UserSeenOffer(std::string offer_id,
                long user_seen_price,
                std::string country_code,
                std::string locale);
  UserSeenOffer(const UserSeenOffer&);
  UserSeenOffer& operator=(const UserSeenOffer&);
  ~UserSeenOffer();

  // Associated offer id.
  std::string offer_id;
  // The price upon subscribing.
  long user_seen_price;
  // Country code of the offer.
  std::string country_code;
  // Locale of the offer.
  std::string locale;
};

extern const int64_t kUnknownSubscriptionTimestamp;
extern const uint64_t kInvalidSubscriptionId;

struct CommerceSubscription {
  // The CommerceSubscription instantiation outside of this subscriptions/
  // component should always use kUnknownSubscriptionTimestamp.
  CommerceSubscription(
      SubscriptionType type,
      IdentifierType id_type,
      std::string id,
      ManagementType management_type,
      int64_t timestamp = kUnknownSubscriptionTimestamp,
      std::optional<UserSeenOffer> user_seen_offer = std::nullopt);
  CommerceSubscription(const CommerceSubscription&);
  CommerceSubscription& operator=(const CommerceSubscription&);
  ~CommerceSubscription();

  SubscriptionType type;
  IdentifierType id_type;
  std::string id;
  ManagementType management_type;
  // The timestamp when the subscription is created on the server side. Upon
  // successful creation on the server side, the valid timestamp will be passed
  // back to client side and then stored locally.
  int64_t timestamp;
  std::optional<UserSeenOffer> user_seen_offer;
};

std::string SubscriptionTypeToString(SubscriptionType type);
SubscriptionType StringToSubscriptionType(const std::string& s);
std::string SubscriptionIdTypeToString(IdentifierType type);
IdentifierType StringToSubscriptionIdType(const std::string& s);
std::string SubscriptionManagementTypeToString(ManagementType type);
ManagementType StringToSubscriptionManagementType(const std::string& s);

// Gets a key for the provided subscription that can be used for storage or
// caching.
std::string GetStorageKeyForSubscription(
    const CommerceSubscription& subscription);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_COMMERCE_SUBSCRIPTION_H_
