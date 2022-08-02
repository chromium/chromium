// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/commerce/core/subscriptions/commerce_subscription.h"

namespace commerce {

UserSeenOffer::UserSeenOffer(std::string offer_id,
                             long user_seen_price,
                             std::string country_code)
    : offer_id(offer_id),
      user_seen_price(user_seen_price),
      country_code(country_code) {}
UserSeenOffer::UserSeenOffer(const UserSeenOffer&) = default;
UserSeenOffer& UserSeenOffer::operator=(const UserSeenOffer&) = default;
UserSeenOffer::~UserSeenOffer() = default;

const int64_t kUnknownSubscriptionTimestamp = 0;

CommerceSubscription::CommerceSubscription(
    SubscriptionType type,
    IdentifierType id_type,
    std::string id,
    ManagementType management_type,
    int64_t timestamp,
    absl::optional<UserSeenOffer> user_seen_offer)
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

}  // namespace commerce
