// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import org.chromium.components.signin.AccountCapabilitiesConstants;
import org.chromium.components.signin.base.AccountCapabilities;

import java.util.HashMap;

/**
 * This class provides a builder for the class {@link AccountCapabilities}.
 *
 * This class is only used in tests.
 */
public final class AccountCapabilitiesBuilder {
    private final HashMap<String, Boolean> mCapabilities = new HashMap<>();

    /** Sets the canOfferExtendedSyncPromos capability value. */
    public AccountCapabilitiesBuilder setCanOfferExtendedSyncPromos(boolean value) {
        mCapabilities.put(
                AccountCapabilitiesConstants.CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS_CAPABILITY_NAME,
                value);
        return this;
    }

    /** @return {@link AccountCapabilities} object with the capabilities set up with the builder. */
    public AccountCapabilities build() {
        return new AccountCapabilities((HashMap<String, Boolean>) mCapabilities.clone());
    }
}
