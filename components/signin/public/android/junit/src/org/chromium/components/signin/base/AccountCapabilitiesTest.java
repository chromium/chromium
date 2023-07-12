// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.base;

import static org.mockito.Mockito.spy;

import com.google.common.collect.Lists;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.params.BlockJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.components.signin.AccountCapabilitiesConstants;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.Tribool;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;

/**
 * Test class for {@link AccountCapabilities}.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
@Config(manifest = Config.NONE)
public final class AccountCapabilitiesTest {
    private FakeAccountManagerDelegate mDelegate;

    /**
     * Returns the capability value for the specified capability name
     * from the appropriate getter in AccountCapabilities.
     * Please keep the list of capabilities alphabetically sorted.
     */
    public static @Tribool int getCapability(
            String capabilityName, AccountCapabilities capabilities) {
        switch (capabilityName) {
            case AccountCapabilitiesConstants.CAN_HAVE_EMAIL_ADDRESS_DISPLAYED_CAPABILITY_NAME:
                return capabilities.canHaveEmailAddressDisplayed();
            case AccountCapabilitiesConstants.CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS_CAPABILITY_NAME:
                return capabilities.canOfferExtendedSyncPromos();
            case AccountCapabilitiesConstants.CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME:
                return capabilities.canRunChromePrivacySandboxTrials();
            case AccountCapabilitiesConstants.IS_OPTED_IN_TO_PARENTAL_SUPERVISION_CAPABILITY_NAME:
                return capabilities.isOptedInToParentalSupervision();
            case AccountCapabilitiesConstants.CAN_TOGGLE_AUTO_UPDATES_NAME:
                return capabilities.canToggleAutoUpdates();
            case AccountCapabilitiesConstants.CAN_USE_CHROME_IP_PROTECTION_NAME:
                return capabilities.canUseChromeIpProtection();
            case AccountCapabilitiesConstants.IS_ALLOWED_FOR_MACHINE_LEARNING_CAPABILITY_NAME:
                return capabilities.isAllowedForMachineLearning();
            case AccountCapabilitiesConstants
                    .IS_SUBJECT_TO_CHROME_PRIVACY_SANDBOX_RESTRICTED_MEASUREMENT_NOTICE:
                return capabilities.isSubjectToChromePrivacySandboxRestrictedMeasurementNotice();
            case AccountCapabilitiesConstants.IS_SUBJECT_TO_ENTERPRISE_POLICIES_CAPABILITY_NAME:
                return capabilities.isSubjectToEnterprisePolicies();
            case AccountCapabilitiesConstants.IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME:
                return capabilities.isSubjectToParentalControls();
        }
        assert false : "Capability name is not known.";
        return -1;
    }

    /** Populates all capabilities with the given response value. */
    public static HashMap<String, Integer> populateCapabilitiesResponse(
            @AccountManagerDelegate.CapabilityResponse int value) {
        HashMap<String, Integer> response = new HashMap<>();
        for (String capabilityName :
                AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES) {
            response.put(capabilityName, value);
        }
        return response;
    }

    /**
     * List of parameters to run in capability fetching tests.
     */
    public static class CapabilitiesTestParams implements ParameterProvider {
        private static List<ParameterSet> sCapabilties = Arrays.asList(
                new ParameterSet()
                        .name("CanHaveEmailAddressDisplayed")
                        .value(AccountCapabilitiesConstants
                                        .CAN_HAVE_EMAIL_ADDRESS_DISPLAYED_CAPABILITY_NAME),
                new ParameterSet()
                        .name("CanOfferExtendedChromeSyncPromos")
                        .value(AccountCapabilitiesConstants
                                        .CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS_CAPABILITY_NAME),
                new ParameterSet()
                        .name("CanRunChromePrivacySandboxTrials")
                        .value(AccountCapabilitiesConstants
                                        .CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME),
                new ParameterSet()
                        .name("IsOptedInToParentalSupervision")
                        .value(AccountCapabilitiesConstants
                                        .IS_OPTED_IN_TO_PARENTAL_SUPERVISION_CAPABILITY_NAME),
                new ParameterSet()
                        .name("CanToggleAutoUpdates")
                        .value(AccountCapabilitiesConstants.CAN_TOGGLE_AUTO_UPDATES_NAME),
                new ParameterSet()
                        .name("CanUseChromeIpProtection")
                        .value(AccountCapabilitiesConstants.CAN_USE_CHROME_IP_PROTECTION_NAME),
                new ParameterSet()
                        .name("IsAllowedForMachineLearning")
                        .value(AccountCapabilitiesConstants
                                        .IS_ALLOWED_FOR_MACHINE_LEARNING_CAPABILITY_NAME),
                new ParameterSet()
                        .name("IsSubjectToEnterprisePolicies")
                        .value(AccountCapabilitiesConstants
                                        .IS_SUBJECT_TO_ENTERPRISE_POLICIES_CAPABILITY_NAME),
                new ParameterSet()
                        .name("IsSubjectToParentalControls")
                        .value(AccountCapabilitiesConstants
                                        .IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME));

        // Returns String value added from Capabilities ParameterSet.
        static String getCapabilityName(ParameterSet parameterSet) {
            String parameterSetString = parameterSet.toString();
            // Removes the brackets added by Arrays.toString for parameter value.
            return parameterSetString.replace("[", "").replace("]", "");
        }

        static {
            // Asserts that the list of parameters contains all supported capability names.
            assert AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES.containsAll(
                    Lists.transform(sCapabilties, (paramSet) -> getCapabilityName(paramSet)));
        }

        @Override
        public Iterable<ParameterSet> getParameters() {
            return sCapabilties;
        }
    }

    @Before
    public void setUp() {
        mDelegate = spy(new FakeAccountManagerDelegate());
    }

    @Test
    @ParameterAnnotations.UseMethodParameter(CapabilitiesTestParams.class)
    public void testCapabilityResponseException(String capabilityName) {
        AccountCapabilities capabilities = new AccountCapabilities(new HashMap<>());
        Assert.assertEquals(getCapability(capabilityName, capabilities), Tribool.UNKNOWN);
    }

    @Test
    @ParameterAnnotations.UseMethodParameter(CapabilitiesTestParams.class)
    public void testCapabilityResponseYes(String capabilityName) {
        AccountCapabilities capabilities = new AccountCapabilities(new HashMap<String, Boolean>() {
            { put(capabilityName, true); }
        });
        Assert.assertEquals(getCapability(capabilityName, capabilities), Tribool.TRUE);
    }

    @Test
    @ParameterAnnotations.UseMethodParameter(CapabilitiesTestParams.class)
    public void testCapabilityResponseNo(String capabilityName) {
        AccountCapabilities capabilities = new AccountCapabilities(new HashMap<String, Boolean>() {
            { put(capabilityName, false); }
        });
        Assert.assertEquals(getCapability(capabilityName, capabilities), Tribool.FALSE);
    }

    @Test
    public void testParseFromCapabilitiesResponseWithResponseYes() {
        AccountCapabilities capabilities = AccountCapabilities.parseFromCapabilitiesResponse(
                populateCapabilitiesResponse(AccountManagerDelegate.CapabilityResponse.YES));

        for (String capabilityName :
                AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES) {
            Assert.assertEquals(getCapability(capabilityName, capabilities), Tribool.TRUE);
        }
    }

    @Test
    public void testParseFromCapabilitiesResponseWithResponseNo() {
        AccountCapabilities capabilities = AccountCapabilities.parseFromCapabilitiesResponse(
                populateCapabilitiesResponse(AccountManagerDelegate.CapabilityResponse.NO));

        for (String capabilityName :
                AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES) {
            Assert.assertEquals(getCapability(capabilityName, capabilities), Tribool.FALSE);
        }
    }

    @Test
    public void testParseFromCapabilitiesResponseWithExceptionResponse() {
        AccountCapabilities capabilities = AccountCapabilities.parseFromCapabilitiesResponse(
                populateCapabilitiesResponse(AccountManagerDelegate.CapabilityResponse.EXCEPTION));

        for (String capabilityName :
                AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES) {
            Assert.assertEquals(getCapability(capabilityName, capabilities), Tribool.UNKNOWN);
        }
    }
}
