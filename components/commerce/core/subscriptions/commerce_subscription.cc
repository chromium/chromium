// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/commerce/core/subscriptions/commerce_subscription.h"

namespace {

const char kSubscriptionTypePriceTrack[] = "PRICE_TRACK";
const char kSubscriptionTypeUnspecified[] = "TYPE_UNSPECIFIED";
const char kSubscriptionIdTypeOfferId[] = "OFFER_ID";
const char kSubscriptionIdTypeProductClusterId[] = "PRODUCT_CLUSTER_ID";
const char kSubscriptionIdTypeUnspecified[] = "IDENTIFIER_TYPE_UNSPECIFIED";
const char kSubscriptionManagementTypeChrome[] = "CHROME_MANAGED";
const char kSubscriptionManagementTypeUser[] = "USER_MANAGED";
const char kSubscriptionManagementTypeUnspecified[] = "TYPE_UNSPECIFIED";

}  // namespace

namespace commerce {

UserSeenOffer::UserSeenOffer(std::string offer_id,
                             long user_seen_price,
                             std::string country_code,
                             std::string locale)
    : offer_id(offer_id),
      user_seen_price(user_seen_price),
      country_code(country_code),
      locale(locale) {}
UserSeenOffer::UserSeenOffer(const UserSeenOffer&) = default;
UserSeenOffer& UserSeenOffer::operator=(const UserSeenOffer&) = default;
UserSeenOffer::~UserSeenOffer() = default;

const int64_t kUnknownSubscriptionTimestamp = 0;
const uint64_t kInvalidSubscriptionId = 0;

CommerceSubscription::CommerceSubscription(
    SubscriptionType type,
    IdentifierType id_type,
    std::string id,
    ManagementType management_type,
    int64_t timestamp,
    std::optional<UserSeenOffer> user_seen_offer)
    : type(type),
      id_type(id_type),
      id(id),
      management_type(management_type),
      timestamp(timestamp),
      user_seen_offer(std::move(user_seen_offer)) {}
CommerceSubscription::CommerceSubscription(const CommerceSubscription&) =
    default;
CommerceSubscription& CommerceSubscription::operator=(
    const CommerceSubscription&) = default;
CommerceSubscription::~CommerceSubscription() = default;

std::string SubscriptionTypeToString(SubscriptionType type) {
  if (SubscriptionType::kPriceTrack == type)
    return kSubscriptionTypePriceTrack;
  else
    return kSubscriptionTypeUnspecified;
}

SubscriptionType StringToSubscriptionType(const std::string& s) {
  if (((std::string)kSubscriptionTypePriceTrack) == s)
    return SubscriptionType::kPriceTrack;
  else
    return SubscriptionType::kTypeUnspecified;
}

std::string SubscriptionIdTypeToString(IdentifierType type) {
  if (IdentifierType::kOfferId == type)
    return kSubscriptionIdTypeOfferId;
  else if (IdentifierType::kProductClusterId == type)
    return kSubscriptionIdTypeProductClusterId;
  else
    return kSubscriptionIdTypeUnspecified;
}

IdentifierType StringToSubscriptionIdType(const std::string& s) {
  if (((std::string)kSubscriptionIdTypeOfferId) == s)
    return IdentifierType::kOfferId;
  else if (((std::string)kSubscriptionIdTypeProductClusterId) == s)
    return IdentifierType::kProductClusterId;
  else
    return IdentifierType::kIdentifierTypeUnspecified;
}

std::string SubscriptionManagementTypeToString(ManagementType type) {
  if (ManagementType::kChromeManaged == type)
    return kSubscriptionManagementTypeChrome;
  else if (ManagementType::kUserManaged == type)
    return kSubscriptionManagementTypeUser;
  else
    return kSubscriptionManagementTypeUnspecified;
}

ManagementType StringToSubscriptionManagementType(const std::string& s) {
  if (((std::string)kSubscriptionManagementTypeChrome) == s)
    return ManagementType::kChromeManaged;
  else if (((std::string)kSubscriptionManagementTypeUser) == s)
    return ManagementType::kUserManaged;
  else
    return ManagementType::kTypeUnspecified;
}

std::string GetStorageKeyForSubscription(
    const CommerceSubscription& subscription) {
  return SubscriptionTypeToString(subscription.type) + "_" +
         SubscriptionIdTypeToString(subscription.id_type) + "_" +
         subscription.id;
}

}  // namespace commerce
