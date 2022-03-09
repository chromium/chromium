// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.base;

import static org.mockito.Mockito.spy;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.signin.AccountCapabilitiesConstants;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.Tribool;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;

import java.util.HashMap;

/**
 * Test class for {@link AccountCapabilities}.
 * TODO(crbug.com/1304162): Refactor to use parameterized test cases.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class AccountCapabilitiesTest {
    private FakeAccountManagerDelegate mDelegate;

    @Before
    public void setUp() {
        mDelegate = spy(new FakeAccountManagerDelegate());
    }

    @Test
    public void testCanRunChromePrivacySandboxTrialsException() {
        AccountCapabilities capabilities = new AccountCapabilities();
        capabilities.setAccountCapability(
                AccountCapabilitiesConstants.CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME,
                AccountManagerDelegate.CapabilityResponse.EXCEPTION);
        Assert.assertEquals(capabilities.canRunChromePrivacySandboxTrials(), Tribool.UNKNOWN);
    }

    @Test
    public void testCanRunChromePrivacySandboxTrialsTrue() {
        AccountCapabilities capabilities = new AccountCapabilities();
        capabilities.setAccountCapability(
                AccountCapabilitiesConstants.CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME,
                AccountManagerDelegate.CapabilityResponse.YES);
        Assert.assertEquals(capabilities.canRunChromePrivacySandboxTrials(), Tribool.TRUE);
    }

    @Test
    public void testCanRunChromePrivacySandboxTrialsFalse() {
        AccountCapabilities capabilities = new AccountCapabilities();
        capabilities.setAccountCapability(
                AccountCapabilitiesConstants.CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME,
                AccountManagerDelegate.CapabilityResponse.NO);
        Assert.assertEquals(capabilities.canRunChromePrivacySandboxTrials(), Tribool.FALSE);
    }

    @Test
    public void testCanRunChromePrivacySandboxTrialsFalseAfterException() {
        AccountCapabilities capabilities = new AccountCapabilities();
        capabilities.setAccountCapability(
                AccountCapabilitiesConstants.CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME,
                AccountManagerDelegate.CapabilityResponse.NO);
        Assert.assertEquals(capabilities.canRunChromePrivacySandboxTrials(), Tribool.FALSE);

        capabilities.setAccountCapability(
                AccountCapabilitiesConstants.CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME,
                AccountManagerDelegate.CapabilityResponse.EXCEPTION);
        Assert.assertEquals(capabilities.canRunChromePrivacySandboxTrials(), Tribool.FALSE);
    }

    @Test
    public void testCanOfferExtendedSyncPromosException() {
        AccountCapabilities capabilities = new AccountCapabilities();
        capabilities.setAccountCapability(
                AccountCapabilitiesConstants.CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS_CAPABILITY_NAME,
                AccountManagerDelegate.CapabilityResponse.EXCEPTION);
        Assert.assertEquals(capabilities.canOfferExtendedSyncPromos(), Tribool.UNKNOWN);
    }

    @Test
    public void testCanOfferExtendedSyncPromosTrue() {
        AccountCapabilities capabilities = new AccountCapabilities();
        capabilities.setAccountCapability(
                AccountCapabilitiesConstants.CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS_CAPABILITY_NAME,
                AccountManagerDelegate.CapabilityResponse.YES);
        Assert.assertEquals(capabilities.canOfferExtendedSyncPromos(), Tribool.TRUE);
    }

    @Test
    public void testCanOfferExtendedSyncPromosFalse() {
        AccountCapabilities capabilities = new AccountCapabilities();
        capabilities.setAccountCapability(
                AccountCapabilitiesConstants.CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS_CAPABILITY_NAME,
                AccountManagerDelegate.CapabilityResponse.NO);
        Assert.assertEquals(capabilities.canOfferExtendedSyncPromos(), Tribool.FALSE);
    }

    @Test
    public void testCanOfferExtendedSyncPromosFalseAfterException() {
        AccountCapabilities capabilities = new AccountCapabilities();
        capabilities.setAccountCapability(
                AccountCapabilitiesConstants.CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS_CAPABILITY_NAME,
                AccountManagerDelegate.CapabilityResponse.NO);
        Assert.assertEquals(capabilities.canOfferExtendedSyncPromos(), Tribool.FALSE);

        capabilities.setAccountCapability(
                AccountCapabilitiesConstants.CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS_CAPABILITY_NAME,
                AccountManagerDelegate.CapabilityResponse.EXCEPTION);
        Assert.assertEquals(capabilities.canOfferExtendedSyncPromos(), Tribool.FALSE);
    }

    @Test
    public void testIsSubjectToParentalControlsException() {
        AccountCapabilities capabilities = new AccountCapabilities();
        capabilities.setAccountCapability(
                AccountCapabilitiesConstants.IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME,
                AccountManagerDelegate.CapabilityResponse.EXCEPTION);
        Assert.assertEquals(capabilities.isSubjectToParentalControls(), Tribool.UNKNOWN);
    }

    @Test
    public void testIsSubjectToParentalControlsTrue() {
        AccountCapabilities capabilities = new AccountCapabilities();
        capabilities.setAccountCapability(
                AccountCapabilitiesConstants.IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME,
                AccountManagerDelegate.CapabilityResponse.YES);
        Assert.assertEquals(capabilities.isSubjectToParentalControls(), Tribool.TRUE);
    }

    @Test
    public void testIsSubjectToParentalControlsFalse() {
        AccountCapabilities capabilities = new AccountCapabilities();
        capabilities.setAccountCapability(
                AccountCapabilitiesConstants.IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME,
                AccountManagerDelegate.CapabilityResponse.NO);
        Assert.assertEquals(capabilities.isSubjectToParentalControls(), Tribool.FALSE);
    }

    @Test
    public void testIsSubjectToParentalControlsFalseAfterException() {
        AccountCapabilities capabilities = new AccountCapabilities();
        capabilities.setAccountCapability(
                AccountCapabilitiesConstants.IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME,
                AccountManagerDelegate.CapabilityResponse.NO);
        Assert.assertEquals(capabilities.isSubjectToParentalControls(), Tribool.FALSE);

        capabilities.setAccountCapability(
                AccountCapabilitiesConstants.IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME,
                AccountManagerDelegate.CapabilityResponse.EXCEPTION);
        Assert.assertEquals(capabilities.isSubjectToParentalControls(), Tribool.FALSE);
    }

    @Test
    public void testParseFromCapabilitiesResponseWithSuccessResponse() {
        AccountCapabilities capabilities =
                AccountCapabilities.parseFromCapabilitiesResponse(new HashMap<String, Integer>() {
                    {
                        put(AccountCapabilitiesConstants
                                        .IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME,
                                AccountManagerDelegate.CapabilityResponse.YES);
                        put(AccountCapabilitiesConstants
                                        .CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS_CAPABILITY_NAME,
                                AccountManagerDelegate.CapabilityResponse.NO);
                        put(AccountCapabilitiesConstants
                                        .CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME,
                                AccountManagerDelegate.CapabilityResponse.NO);
                    }
                });
        Assert.assertEquals(capabilities.canOfferExtendedSyncPromos(), Tribool.FALSE);
        Assert.assertEquals(capabilities.isSubjectToParentalControls(), Tribool.TRUE);
        Assert.assertEquals(capabilities.canRunChromePrivacySandboxTrials(), Tribool.FALSE);
    }

    @Test
    public void testParseFromCapabilitiesResponseWithExceptionResponse() {
        AccountCapabilities capabilities =
                AccountCapabilities.parseFromCapabilitiesResponse(new HashMap<String, Integer>() {
                    {
                        put(AccountCapabilitiesConstants
                                        .IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME,
                                AccountManagerDelegate.CapabilityResponse.EXCEPTION);
                        put(AccountCapabilitiesConstants
                                        .CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS_CAPABILITY_NAME,
                                AccountManagerDelegate.CapabilityResponse.EXCEPTION);
                        put(AccountCapabilitiesConstants
                                        .CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME,
                                AccountManagerDelegate.CapabilityResponse.EXCEPTION);
                    }
                });
        Assert.assertEquals(capabilities.canOfferExtendedSyncPromos(), Tribool.UNKNOWN);
        Assert.assertEquals(capabilities.isSubjectToParentalControls(), Tribool.UNKNOWN);
        Assert.assertEquals(capabilities.canRunChromePrivacySandboxTrials(), Tribool.UNKNOWN);
    }
}
