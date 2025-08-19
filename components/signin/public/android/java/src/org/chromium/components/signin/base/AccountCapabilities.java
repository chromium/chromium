// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.base;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.signin.AccountCapabilitiesConstants;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.Tribool;

import java.util.HashMap;
import java.util.Map;

/**
 * Stores the state associated with supported account capabilities. This class has a native
 * counterpart.
 */
@NullMarked
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
     * Returns the supported account capabilities values.
     *
     * @param capabilityResponses the mapping from capability name to value.
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

    /** keep-sorted start block=yes sticky_prefixes=/**,* newline_separated=yes */
    /** Returns canFetchFamilyMemberInfo capability value. */
    public @Tribool int canFetchFamilyMemberInfo() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_FETCH_FAMILY_MEMBER_INFO_CAPABILITY_NAME);
    }

    /** Returns canHaveEmailAddressDisplayed capability value. */
    public @Tribool int canHaveEmailAddressDisplayed() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_HAVE_EMAIL_ADDRESS_DISPLAYED_CAPABILITY_NAME);
    }

    /** Returns canRunChromePrivacySandboxTrials capability value. */
    public @Tribool int canRunChromePrivacySandboxTrials() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME);
    }

    /** Returns canShowHistorySyncOptInsWithoutMinorModeRestrictions capability value. */
    public @Tribool int canShowHistorySyncOptInsWithoutMinorModeRestrictions() {
        return getCapabilityByName(
                AccountCapabilitiesConstants
                        .CAN_SHOW_HISTORY_SYNC_OPT_INS_WITHOUT_MINOR_MODE_RESTRICTIONS_CAPABILITY_NAME);
    }

    /** Returns canToggleAutoUpdates capability value. */
    public @Tribool int canToggleAutoUpdates() {
        return getCapabilityByName(AccountCapabilitiesConstants.CAN_TOGGLE_AUTO_UPDATES_NAME);
    }

    /** Returns canUseChromeIpProtection capability value. */
    public @Tribool int canUseChromeIpProtection() {
        return getCapabilityByName(AccountCapabilitiesConstants.CAN_USE_CHROME_IP_PROTECTION_NAME);
    }

    /** Returns canUseChromeOSGenerativeAi capability value. */
    public @Tribool int canUseChromeOSGenerativeAi() {
        return getCapabilityByName(AccountCapabilitiesConstants.CAN_USE_CHROMEOS_GENERATIVE_AI);
    }

    /** Returns canUseCopyeditorFeature capability value. */
    public @Tribool int canUseCopyeditorFeature() {
        return getCapabilityByName(AccountCapabilitiesConstants.CAN_USE_COPYEDITOR_FEATURE_NAME);
    }

    /** Returns canUseDevToolsGenerativeAiFeatures capability value. */
    public @Tribool int canUseDevToolsGenerativeAiFeatures() {
        return getCapabilityByName(
                AccountCapabilitiesConstants
                        .CAN_USE_DEVTOOLS_GENERATIVE_AI_FEATURES_CAPABILITY_NAME);
    }

    /** Returns canUseEduFeatures capability value. */
    public @Tribool int canUseEduFeatures() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_USE_EDU_FEATURES_CAPABILITY_NAME);
    }

    /** Returns canUseGenerativeAiInRecorderApp capability value. */
    public @Tribool int canUseGenerativeAiInRecorderApp() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_USE_GENERATIVE_AI_IN_RECORDER_APP);
    }

    /** Returns canUseGenerativeAiPhotoEditing capability value. */
    public @Tribool int canUseGenerativeAiPhotoEditing() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_USE_GENERATIVE_AI_PHOTO_EDITING);
    }

    /** Returns canUseMantaService capability value. */
    public @Tribool int canUseMantaService() {
        return getCapabilityByName(AccountCapabilitiesConstants.CAN_USE_MANTA_SERVICE_NAME);
    }

    /** Returns canUseModelExecutionFeatures capability value. */
    public @Tribool int canUseModelExecutionFeatures() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_USE_MODEL_EXECUTION_FEATURES_NAME);
    }

    /** Returns canUseSpeakerLabelInRecorderApp capability value. */
    public @Tribool int canUseSpeakerLabelInRecorderApp() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.CAN_USE_SPEAKER_LABEL_IN_RECORDER_APP);
    }

    /** Returns isAllowedForMachineLearning capability value. */
    public @Tribool int isAllowedForMachineLearning() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.IS_ALLOWED_FOR_MACHINE_LEARNING_CAPABILITY_NAME);
    }

    /** Returns isOptedInToParentalSupervision capability value. */
    public @Tribool int isOptedInToParentalSupervision() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.IS_OPTED_IN_TO_PARENTAL_SUPERVISION_CAPABILITY_NAME);
    }

    /** Returns isSubjectToChromePrivacySandboxRestrictedMeasurementNotice capability value. */
    public @Tribool int isSubjectToChromePrivacySandboxRestrictedMeasurementNotice() {
        return getCapabilityByName(
                AccountCapabilitiesConstants
                        .IS_SUBJECT_TO_CHROME_PRIVACY_SANDBOX_RESTRICTED_MEASUREMENT_NOTICE);
    }

    /** Returns isSubjectToEnterpriseFeatures capability value. */
    public @Tribool int isSubjectToEnterpriseFeatures() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.IS_SUBJECT_TO_ENTERPRISE_POLICIES_CAPABILITY_NAME);
    }

    /** Returns isSubjectToParentalControls capability value. */
    public @Tribool int isSubjectToParentalControls() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME);
    }

    /** Returns shouldBeAddressedInFeminineGrammaticalGender capability value. */
    public @Tribool int shouldBeAddressedInFeminineGrammaticalGender() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.SHOULD_BE_ADDRESSED_IN_FEMININE_GRAMMATICAL_GENDER);
    }

    /** Returns shouldBeAddressedInMasculineGrammaticalGender capability value. */
    public @Tribool int shouldBeAddressedInMasculineGrammaticalGender() {
        return getCapabilityByName(
                AccountCapabilitiesConstants.SHOULD_BE_ADDRESSED_IN_MASCULINE_GRAMMATICAL_GENDER);
    }

    /** keep-sorted end */

    /**
     * Merges capabilities from another {@link AccountCapabilities} object into this one.
     *
     * <p>New capabilities that were not already set are added and existing ones are updated with
     * the new values.
     *
     * @param other The {@link AccountCapabilities} object to merge values from.
     * @return true if this object's capabilities were changed as a result of the update and false
     *     otherwise.
     */
    public boolean updateWith(AccountCapabilities other) {
        boolean modified = false;
        for (Map.Entry<String, Boolean> otherCapability : other.mAccountCapabilities.entrySet()) {
            String name = otherCapability.getKey();
            Boolean value = otherCapability.getValue();
            if (mAccountCapabilities.containsKey(name)
                    && mAccountCapabilities.get(name).equals(value)) {
                continue;
            }
            mAccountCapabilities.put(name, value);
            modified = true;
        }
        return modified;
    }

    /**
     * Returns the capability value associated to the name.
     *
     * @param capabilityName the name of the capability to lookup.
     */
    @CalledByNative
    private @Tribool int getCapabilityByName(String capabilityName) {
        if (!mAccountCapabilities.containsKey(capabilityName)) {
            return Tribool.UNKNOWN;
        }
        return mAccountCapabilities.get(capabilityName) ? Tribool.TRUE : Tribool.FALSE;
    }
}
