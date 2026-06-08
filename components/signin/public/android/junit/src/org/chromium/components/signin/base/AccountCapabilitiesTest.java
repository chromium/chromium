// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.base;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.components.signin.AccountCapabilitiesConstants;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.Tribool;

import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Test class for {@link AccountCapabilities}. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class AccountCapabilitiesTest {

    @Rule public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Parameter(0)
    public String mCapabilityName;

    /**
     * Returns the capability value for the specified capability name from the appropriate getter in
     * AccountCapabilities. Please keep the list of capabilities alphabetically sorted.
     */
    public static @Tribool int getCapability(
            String capabilityName, AccountCapabilities capabilities) {
        switch (capabilityName) {
            /** keep-sorted start */
            case AccountCapabilitiesConstants.CAN_FETCH_FAMILY_MEMBER_INFO_CAPABILITY_NAME:
                return capabilities.canFetchFamilyMemberInfo();
            case AccountCapabilitiesConstants.CAN_HAVE_EMAIL_ADDRESS_DISPLAYED_CAPABILITY_NAME:
                return capabilities.canHaveEmailAddressDisplayed();
            case AccountCapabilitiesConstants.CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME:
                return capabilities.canRunChromePrivacySandboxTrials();
            case AccountCapabilitiesConstants
                    .CAN_SHOW_HISTORY_SYNC_OPT_INS_WITHOUT_MINOR_MODE_RESTRICTIONS_CAPABILITY_NAME:
                return capabilities.canShowHistorySyncOptInsWithoutMinorModeRestrictions();
            case AccountCapabilitiesConstants
                    .CAN_USE_DEVTOOLS_GENERATIVE_AI_FEATURES_CAPABILITY_NAME:
                return capabilities.canUseDevToolsGenerativeAiFeatures();
            case AccountCapabilitiesConstants.CAN_USE_EDU_FEATURES_CAPABILITY_NAME:
                return capabilities.canUseEduFeatures();
            case AccountCapabilitiesConstants.CAN_USE_GEMINI_IN_CHROME_CAPABILITY_NAME:
                return capabilities.canUseGeminiInChromeCapability();
            case AccountCapabilitiesConstants.CAN_USE_MANTA_SERVICE_NAME:
                return capabilities.canUseMantaService();
            case AccountCapabilitiesConstants.CAN_USE_MODEL_EXECUTION_FEATURES_NAME:
                return capabilities.canUseModelExecutionFeatures();
            case AccountCapabilitiesConstants.CAN_USE_SPEAKER_LABEL_IN_RECORDER_APP:
                return capabilities.canUseSpeakerLabelInRecorderApp();
            case AccountCapabilitiesConstants.IS_ALLOWED_FOR_MACHINE_LEARNING_CAPABILITY_NAME:
                return capabilities.isAllowedForMachineLearning();
            case AccountCapabilitiesConstants.IS_OPTED_IN_TO_PARENTAL_SUPERVISION_CAPABILITY_NAME:
                return capabilities.isOptedInToParentalSupervision();
            case AccountCapabilitiesConstants
                    .IS_SUBJECT_TO_CHROME_PRIVACY_SANDBOX_RESTRICTED_MEASUREMENT_NOTICE:
                return capabilities.isSubjectToChromePrivacySandboxRestrictedMeasurementNotice();
            case AccountCapabilitiesConstants.IS_SUBJECT_TO_ENTERPRISE_POLICIES_CAPABILITY_NAME:
                return capabilities.isSubjectToEnterpriseFeatures();
            case AccountCapabilitiesConstants.IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME:
                return capabilities.isSubjectToParentalControls();
            case AccountCapabilitiesConstants.SUPPORTS_WALLET_PRIVATE_PASSES_IN_AUTOFILL_NAME:
                return capabilities.supportsWalletPrivatePassesInAutofill();
                /** keep-sorted end */
        }
        throw new AssertionError("Capability name is not known.");
    }

    /** Populates all capabilities with the given response value. */
    public static Map<String, Integer> populateCapabilitiesResponse(
            @AccountManagerDelegate.CapabilityResponse int value) {
        HashMap<String, Integer> response = new HashMap<>();
        for (String capabilityName :
                AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES) {
            response.put(capabilityName, value);
        }
        return response;
    }

    @Parameters
    public static Collection<Object> data() {
        List<Object> params =
                Arrays.asList(
                        new Object[] {
                            AccountCapabilitiesConstants
                                    .CAN_HAVE_EMAIL_ADDRESS_DISPLAYED_CAPABILITY_NAME,
                            AccountCapabilitiesConstants
                                    .CAN_SHOW_HISTORY_SYNC_OPT_INS_WITHOUT_MINOR_MODE_RESTRICTIONS_CAPABILITY_NAME,
                            AccountCapabilitiesConstants
                                    .CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME,
                            AccountCapabilitiesConstants
                                    .IS_OPTED_IN_TO_PARENTAL_SUPERVISION_CAPABILITY_NAME,
                            AccountCapabilitiesConstants
                                    .CAN_USE_DEVTOOLS_GENERATIVE_AI_FEATURES_CAPABILITY_NAME,
                            AccountCapabilitiesConstants.CAN_USE_MANTA_SERVICE_NAME,
                            AccountCapabilitiesConstants.CAN_USE_MODEL_EXECUTION_FEATURES_NAME,
                            AccountCapabilitiesConstants
                                    .IS_ALLOWED_FOR_MACHINE_LEARNING_CAPABILITY_NAME,
                            AccountCapabilitiesConstants
                                    .IS_SUBJECT_TO_ENTERPRISE_POLICIES_CAPABILITY_NAME,
                            AccountCapabilitiesConstants
                                    .CAN_FETCH_FAMILY_MEMBER_INFO_CAPABILITY_NAME,
                            AccountCapabilitiesConstants.CAN_USE_GEMINI_IN_CHROME_CAPABILITY_NAME,
                            AccountCapabilitiesConstants
                                    .IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME,
                            AccountCapabilitiesConstants.CAN_USE_SPEAKER_LABEL_IN_RECORDER_APP,
                            AccountCapabilitiesConstants
                                    .SUPPORTS_WALLET_PRIVATE_PASSES_IN_AUTOFILL_NAME
                        });
        assert AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES.containsAll(params);
        return params;
    }

    @Test
    public void testCapabilityResponseException() {
        AccountCapabilities capabilities = new AccountCapabilities(Map.of());
        Assert.assertEquals(Tribool.UNKNOWN, getCapability(mCapabilityName, capabilities));
    }

    @Test
    public void testCapabilityResponseYes() {
        AccountCapabilities capabilities = new AccountCapabilities(Map.of(mCapabilityName, true));
        Assert.assertEquals(Tribool.TRUE, getCapability(mCapabilityName, capabilities));
    }

    @Test
    public void testCapabilityResponseNo() {
        AccountCapabilities capabilities = new AccountCapabilities(Map.of(mCapabilityName, false));
        Assert.assertEquals(Tribool.FALSE, getCapability(mCapabilityName, capabilities));
    }

    @Test
    @DisabledTest(message = "https://crbug.com/501072493")
    public void testParseFromCapabilitiesResponseWithResponseYes() {
        AccountCapabilities capabilities =
                AccountCapabilities.parseFromCapabilitiesResponse(
                        populateCapabilitiesResponse(
                                AccountManagerDelegate.CapabilityResponse.YES));

        for (String capabilityName :
                AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES) {
            Assert.assertEquals(Tribool.TRUE, getCapability(capabilityName, capabilities));
        }
    }

    @Test
    @DisabledTest(message = "https://crbug.com/501072493")
    public void testParseFromCapabilitiesResponseWithResponseNo() {
        AccountCapabilities capabilities =
                AccountCapabilities.parseFromCapabilitiesResponse(
                        populateCapabilitiesResponse(AccountManagerDelegate.CapabilityResponse.NO));

        for (String capabilityName :
                AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES) {
            Assert.assertEquals(Tribool.FALSE, getCapability(capabilityName, capabilities));
        }
    }

    @Test
    @DisabledTest(message = "https://crbug.com/501072493")
    public void testParseFromCapabilitiesResponseWithExceptionResponse() {
        AccountCapabilities capabilities =
                AccountCapabilities.parseFromCapabilitiesResponse(
                        populateCapabilitiesResponse(
                                AccountManagerDelegate.CapabilityResponse.EXCEPTION));

        for (String capabilityName :
                AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES) {
            Assert.assertEquals(Tribool.UNKNOWN, getCapability(capabilityName, capabilities));
        }
    }
}
