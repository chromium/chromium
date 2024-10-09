// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.base;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

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
                    : "Capability name not supported in Chrome: " + capabilityName;
            mAccountCapabilities.put(capabilityName, capabilityValues[i]);
        }
    }

    @VisibleForTesting
    public AccountCapabilities(Map<String, Boolean> accountCapabilities) {
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
        Map<String, Boolean> capabilities = new HashMap<>();
        for (String capabilityName :
                AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES) {
            assert capabilityResponses.containsKey(capabilityName);
            @AccountManagerDelegate.CapabilityResponse
            int hasCapability = capabilityResponses.get(capabilityName);
            if (hasCapability != AccountManagerDelegate.CapabilityResponse.EXCEPTION) {
                capabilities.put(
                        capabilityName,
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
     * @return canShowHistorySyncOptInsWithoutMinorModeRestrictions capability value.
     */
    public @Tribool int canShowHistorySyncOptInsWithoutMinorModeRestrictions() {
        return getCapabilityByName(
                AccountCapabilitiesConstants
                        .CAN_SHOW_HISTORY_SYNC_OPT_INS_WITHOUT_MINOR_MODE_RESTRICTIONS_CAPABILITY_NAME);
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
     * @return canFetchFamilyMemberInfo capability value.
     */
    public @Tribool int canFetchFamilyMemberInfo() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_FETCH_FAMILY_MEMBER_INFO_CAPABILITY_NAME);
    }

    /**
     * @return canToggleAutoUpdates capability value.
     */
    public @Tribool int canToggleAutoUpdates() {
        return getCapabilityByName(AccountCapabilitiesConstants.CAN_TOGGLE_AUTO_UPDATES_NAME);
    }

    /**
     * @return canUseChromeIpProtection capability value.
     */
    public @Tribool int canUseChromeIpProtection() {
        return getCapabilityByName(AccountCapabilitiesConstants.CAN_USE_CHROME_IP_PROTECTION_NAME);
    }

    /**
     * @return canUseCopyeditorFeature capability value.
     */
    public @Tribool int canUseCopyeditorFeature() {
        return getCapabilityByName(AccountCapabilitiesConstants.CAN_USE_COPYEDITOR_FEATURE_NAME);
    }

    /**
     * @return canUseDevToolsGenerativeAiFeatures capability value.
     */
    public @Tribool int canUseDevToolsGenerativeAiFeatures() {
        return getCapabilityByName(
                AccountCapabilitiesConstants
                        .CAN_USE_DEVTOOLS_GENERATIVE_AI_FEATURES_CAPABILITY_NAME);
    }

    /**
     * @return canUseEduFeatures capability value.
     */
    public @Tribool int canUseEduFeatures() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_USE_EDU_FEATURES_CAPABILITY_NAME);
    }

    /**
     * @return canUseMantaService capability value.
     */
    public @Tribool int canUseMantaService() {
        return getCapabilityByName(AccountCapabilitiesConstants.CAN_USE_MANTA_SERVICE_NAME);
    }

    /**
     * @return canUseModelExecutionFeatures capability value.
     */
    public @Tribool int canUseModelExecutionFeatures() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_USE_MODEL_EXECUTION_FEATURES_NAME);
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
     * @return canUseSpeakerLabelInRecorderApp capability value.
     */
    public @Tribool int canUseSpeakerLabelInRecorderApp() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_USE_SPEAKER_LABEL_IN_RECORDER_APP);
    }

    /**
     * @return canUseGenerativeAiInRecorderApp capability value.
     */
    public @Tribool int canUseGenerativeAiInRecorderApp() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_USE_GENERATIVE_AI_IN_RECORDER_APP);
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
