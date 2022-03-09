// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.base;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.signin.AccountCapabilitiesConstants;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.Tribool;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Stores the state associated with supported account capabilities.
 * This class has a native counterpart.
 */
public class AccountCapabilities {
    // List of supported account capabilities generated from account_capabilities_constants.h.
    public static final Set<String> SUPPORTED_ACCOUNT_CAPABILITY_NAMES = new HashSet<String>() {
        { add(AccountCapabilitiesConstants.IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME); }
        { add(AccountCapabilitiesConstants.CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS_CAPABILITY_NAME); }
        { add(AccountCapabilitiesConstants.CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME); }
    };

    private final Map<String, Boolean> mAccountCapabilities = new HashMap<>();

    @CalledByNative
    public AccountCapabilities(String[] capabilityNames, boolean[] capabilityValues) {
        assert capabilityNames.length == capabilityValues.length;
        for (int i = 0; i < capabilityNames.length; i += 1) {
            final String capabilityName = capabilityNames[i];
            assert SUPPORTED_ACCOUNT_CAPABILITY_NAMES.contains(capabilityName)
                : "Capability name not supported in Chrome: "
                    + capabilityName;
            mAccountCapabilities.put(capabilityName, capabilityValues[i]);
        }
    }

    public AccountCapabilities() {}

    /**
     * @param account the given account to retrieve capabilities from.
     * @param managerDelegate the manager used to query capability responses.
     * @return the supported account capabilities values.
     */
    public static AccountCapabilities parseFromCapabilitiesResponse(
            Map<String, Integer> capabilityResponses) {
        assert capabilityResponses.size() == SUPPORTED_ACCOUNT_CAPABILITY_NAMES.size();
        AccountCapabilities capabilities = new AccountCapabilities();
        for (String capabilityName : SUPPORTED_ACCOUNT_CAPABILITY_NAMES) {
            assert capabilityResponses.containsKey(capabilityName);
            @AccountManagerDelegate.CapabilityResponse
            int hasCapability = capabilityResponses.get(capabilityName);
            capabilities.setAccountCapability(capabilityName, hasCapability);
        }
        return capabilities;
    }

    /**
     * Stores the Capability Value for the given capability name.
     * @param capabilityName One of the supported capability names {@link
     *         #SUPPORTED_ACCOUNT_CAPABILITY_NAMES}.
     * @param hasCapability Capability Response for this capability.
     */
    public void setAccountCapability(@NonNull String capabilityName,
            @AccountManagerDelegate.CapabilityResponse int hasCapability) {
        assert SUPPORTED_ACCOUNT_CAPABILITY_NAMES.contains(capabilityName)
            : "Capability name not supported: "
                + capabilityName;
        if (hasCapability == AccountManagerDelegate.CapabilityResponse.EXCEPTION) {
            /* The value of the capability is unknown, no need to change the map of capabilities. */
            return;
        }
        mAccountCapabilities.put(
                capabilityName, hasCapability == AccountManagerDelegate.CapabilityResponse.YES);
    }

    /**
     * @return canOfferExtendedSyncPromos capability value.
     */
    public @Tribool int canOfferExtendedSyncPromos() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS_CAPABILITY_NAME);
    }

    /**
     * @return isSubjectToParentalControls capability value.
     */
    public @Tribool int isSubjectToParentalControls() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME);
    }

    /**
     * @return canRunChromePrivacySandboxTrials capability value.
     */
    public @Tribool int canRunChromePrivacySandboxTrials() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME);
    }

    /**
     * @param capabilityName the name of the capability to lookup.
     * @return the capability value associated to the name.
     */
    @CalledByNative
    private @Tribool int getCapabilityByName(@NonNull String capabilityName) {
        if (!mAccountCapabilities.containsKey(capabilityName)) {
            return Tribool.UNKNOWN;
        }
        return mAccountCapabilities.get(capabilityName) ? Tribool.TRUE : Tribool.FALSE;
    }
}
