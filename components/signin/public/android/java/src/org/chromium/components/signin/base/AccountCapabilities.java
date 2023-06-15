// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.base;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.signin.AccountCapabilitiesConstants;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.Tribool;

import java.util.HashMap;
import java.util.Map;

/**
 * Stores the state associated with supported account capabilities. This class has a native
 * counterpart.
 */
public class AccountCapabilities {
    private final Map<String, Boolean> mAccountCapabilities;

    @CalledByNative
    public AccountCapabilities(String[] capabilityNames, boolean[] capabilityValues) {
        mAccountCapabilities = new HashMap<>();
        assert capabilityNames.length == capabilityValues.length;
        for (int i = 0; i < capabilityNames.length; i += 1) {
            final String capabilityName = capabilityNames[i];
            assert AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES.contains(
                    capabilityName)
                : "Capability name not supported in Chrome: "
                    + capabilityName;
            mAccountCapabilities.put(capabilityName, capabilityValues[i]);
        }
    }

    @VisibleForTesting
    public AccountCapabilities(HashMap<String, Boolean> accountCapabilities) {
        mAccountCapabilities = accountCapabilities;
    }

    /**
     * @param capabilityResponses the mapping from capability name to value.
     * @return the supported account capabilities values.
     */
    public static AccountCapabilities parseFromCapabilitiesResponse(
            Map<String, Integer> capabilityResponses) {
        assert capabilityResponses.size()
                == AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES.size();
        HashMap<String, Boolean> capabilities = new HashMap<>();
        for (String capabilityName :
                AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES) {
            assert capabilityResponses.containsKey(capabilityName);
            @AccountManagerDelegate.CapabilityResponse
            int hasCapability = capabilityResponses.get(capabilityName);
            if (hasCapability != AccountManagerDelegate.CapabilityResponse.EXCEPTION) {
                capabilities.put(capabilityName,
                        hasCapability == AccountManagerDelegate.CapabilityResponse.YES);
            }
        }
        return new AccountCapabilities(capabilities);
    }

    /** Please keep the list of capability getters alphabetically sorted. */

    /**
     * @return canHaveEmailAddressDisplayed capability value.
     */
    public @Tribool int canHaveEmailAddressDisplayed() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_HAVE_EMAIL_ADDRESS_DISPLAYED_CAPABILITY_NAME);
    }

    /**
     * @return canOfferExtendedSyncPromos capability value.
     */
    public @Tribool int canOfferExtendedSyncPromos() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS_CAPABILITY_NAME);
    }

    /**
     * @return canRunChromePrivacySandboxTrials capability value.
     */
    public @Tribool int canRunChromePrivacySandboxTrials() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME);
    }

    /**
     * @return isOptedInToParentalSupervision capability value.
     */
    public @Tribool int isOptedInToParentalSupervision() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.IS_OPTED_IN_TO_PARENTAL_SUPERVISION_CAPABILITY_NAME);
    }

    /**
     * @return canToggleAutoUpdates capability value.
     */
    public @Tribool int canToggleAutoUpdates() {
        return getCapabilityByName(AccountCapabilitiesConstants.CAN_TOGGLE_AUTO_UPDATES_NAME);
    }

    /**
     * @return isAllowedForMachineLearning capability value.
     */
    public @Tribool int isAllowedForMachineLearning() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.IS_ALLOWED_FOR_MACHINE_LEARNING_CAPABILITY_NAME);
    }

    /**
     * @return isSubjectToChromePrivacySandboxRestrictedMeasurementNotice capability value.
     */
    public @Tribool int isSubjectToChromePrivacySandboxRestrictedMeasurementNotice() {
        return getCapabilityByName(
                AccountCapabilitiesConstants
                        .IS_SUBJECT_TO_CHROME_PRIVACY_SANDBOX_RESTRICTED_MEASUREMENT_NOTICE);
    }

    /**
     * @return isSubjectToEnterprisePolicies capability value.
     */
    public @Tribool int isSubjectToEnterprisePolicies() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.IS_SUBJECT_TO_ENTERPRISE_POLICIES_CAPABILITY_NAME);
    }

    /**
     * @return isSubjectToParentalControls capability value.
     */
    public @Tribool int isSubjectToParentalControls() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME);
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
