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

    /** Sets the canShowHistorySyncOptInsWithoutMinorModeRestrictions capability value. */
    public AccountCapabilitiesBuilder setCanShowHistorySyncOptInsWithoutMinorModeRestrictions(
            boolean value) {
        mCapabilities.put(
                AccountCapabilitiesConstants
                        .CAN_SHOW_HISTORY_SYNC_OPT_INS_WITHOUT_MINOR_MODE_RESTRICTIONS_CAPABILITY_NAME,
                value);
        return this;
    }

    /** Sets the isSubjectToParentalControls capability value. */
    public AccountCapabilitiesBuilder setIsSubjectToParentalControls(boolean value) {
        mCapabilities.put(
                AccountCapabilitiesConstants.IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME,
                value);
        return this;
    }

    /** Sets the canHaveEmailAddressDisplayed capability value. */
    public AccountCapabilitiesBuilder setCanHaveEmailAddressDisplayed(boolean value) {
        mCapabilities.put(
                AccountCapabilitiesConstants.CAN_HAVE_EMAIL_ADDRESS_DISPLAYED_CAPABILITY_NAME,
                value);
        return this;
    }

    /**
     * @return {@link AccountCapabilities} object with the capabilities set up with the builder.
     */
    public AccountCapabilities build() {
        return new AccountCapabilities((HashMap<String, Boolean>) mCapabilities.clone());
    }
}
