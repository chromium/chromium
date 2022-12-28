// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.commerce.core;

import androidx.annotation.Nullable;

/** Represents the information for one commerce subscription entry. */
public class CommerceSubscription {
    public final @SubscriptionType int type;
    public final @IdentifierType int idType;
    public final String id;
    public final @ManagementType int managementType;
    @Nullable
    public final UserSeenOffer userSeenOffer;

    /** User seen offer data upon price tracking subscribing. */
    public static class UserSeenOffer {
        /** Associated offer id. */
        public final String offerId;
        /** The price upon subscribing. */
        public final long userSeenPrice;
        /** Country code of the offer. */
        public final String countryCode;

        public UserSeenOffer(String offerId, long userSeenPrice, String countryCode) {
            this.offerId = offerId;
            this.userSeenPrice = userSeenPrice;
            this.countryCode = countryCode;
        }
    }

    public CommerceSubscription(@SubscriptionType int type, @IdentifierType int idType, String id,
            @ManagementType int managementType, @Nullable UserSeenOffer userSeenOffer) {
        this.type = type;
        this.idType = idType;
        this.id = id;
        this.managementType = managementType;
        this.userSeenOffer = userSeenOffer;
    }
}
