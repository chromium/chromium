// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.commerce.core;

import android.text.TextUtils;

import androidx.annotation.Nullable;

/** Represents the information for one commerce subscription entry. */
public class CommerceSubscription {
    public final @SubscriptionType int type;
    public final @IdentifierType int idType;
    public final String id;
    public final @ManagementType int managementType;
    @Nullable public final UserSeenOffer userSeenOffer;

    /** User seen offer data upon price tracking subscribing. */
    public static class UserSeenOffer {
        /** Associated offer id. */
        public final String offerId;

        /** The price upon subscribing. */
        public final long userSeenPrice;

        /** Country code of the offer. */
        public final String countryCode;

        /** Locale of the offer. */
        public final String locale;

        public UserSeenOffer(
                String offerId, long userSeenPrice, String countryCode, String locale) {
            this.offerId = offerId;
            this.userSeenPrice = userSeenPrice;
            this.countryCode = countryCode;
            this.locale = locale;
        }
    }

    public CommerceSubscription(
            @SubscriptionType int type,
            @IdentifierType int idType,
            String id,
            @ManagementType int managementType,
            @Nullable UserSeenOffer userSeenOffer) {
        this.type = type;
        this.idType = idType;
        this.id = id;
        this.managementType = managementType;
        this.userSeenOffer = userSeenOffer;
    }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof CommerceSubscription)) return false;

        CommerceSubscription sub = (CommerceSubscription) other;

        // We intentionally don't check userSeenOffer since it's not considered important in
        // uniquely identifying a subscription.
        return sub.type == type
                && sub.idType == idType
                && TextUtils.equals(sub.id, id)
                && sub.managementType == managementType;
    }

    @Override
    public int hashCode() {
        // Use arbitrary primes to seed each step.
        int hash = 37;
        hash = 43 * hash + type;
        hash = 47 * hash + idType;
        hash = 53 * hash + (id == null ? 0 : id.hashCode());
        hash = 59 * hash + managementType;

        // We intentionally don't check userSeenOffer since it's not considered important in
        // uniquely identifying a subscription.
        return hash;
    }
}
