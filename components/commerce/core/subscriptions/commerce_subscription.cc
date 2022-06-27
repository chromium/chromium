// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/commerce/core/subscriptions/commerce_subscription.h"

namespace commerce {

UserSeenOffer::UserSeenOffer(uint64_t offer_id,
                             long user_seen_price,
                             const std::string& country_code)
    : offer_id(offer_id),
      user_seen_price(user_seen_price),
      country_code(country_code) {}
UserSeenOffer::~UserSeenOffer() = default;

CommerceSubscription::CommerceSubscription(SubscriptionType type,
                                           IdentifierType id_type,
                                           uint64_t id,
                                           ManagementType management_type)
    : type(type),
      id_type(id_type),
      id(id),
      management_type(management_type),
      user_seen_offer(absl::nullopt),
      timestamp(0) {}

CommerceSubscription::CommerceSubscription(
    SubscriptionType type,
    IdentifierType id_type,
    uint64_t id,
    ManagementType management_type,
    absl::optional<UserSeenOffer> user_seen_offer)
    : type(type),
      id_type(id_type),
      id(id),
      management_type(management_type),
      user_seen_offer(std::move(user_seen_offer)),
      timestamp(0) {}
CommerceSubscription::~CommerceSubscription() = default;

}  // namespace commerce
