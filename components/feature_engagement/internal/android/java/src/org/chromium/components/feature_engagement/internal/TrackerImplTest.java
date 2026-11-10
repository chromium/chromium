// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement.internal;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.FeatureProperties;
import org.chromium.components.feature_engagement.TriggerDetails;
import org.chromium.components.feature_engagement.TriggerHelpUIResult;
import org.chromium.components.feature_engagement.UiFormat;

/** Unit tests for {@link TrackerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TrackerImplTest {
    private static final long NATIVE_PTR = 1L;
    private static final String TEST_FEATURE = "IPH_TestFeature";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TrackerImpl.Natives mTrackerJniMock;

    private TrackerImpl mTracker;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        TrackerImplJni.setInstanceForTesting(mTrackerJniMock);
        mTracker = TrackerImpl.create(NATIVE_PTR);
        mTracker.setInterruptiveCooldownGroupEnabledSupplier(
                SupplierUtils.alwaysTrueBooleanSupplier());
        mTracker.setBlueBubbleCooldownGroupEnabledSupplier(
                SupplierUtils.alwaysTrueBooleanSupplier());
        mUserActionTester = new UserActionTester();

        // Default: feature and cooldown groups pass wouldTriggerHelpUi and shouldTriggerHelpUi.
        when(mTrackerJniMock.wouldTriggerHelpUi(anyLong(), anyString())).thenReturn(true);
        when(mTrackerJniMock.shouldTriggerHelpUi(anyLong(), anyString())).thenReturn(true);
        when(mTrackerJniMock.shouldTriggerHelpUiWithSnooze(anyLong(), anyString()))
                .thenReturn(new TriggerDetails(true, true));
    }

    @After
    public void tearDown() {
        mUserActionTester.tearDown();
    }

    @Test
    public void testFailsOwnRules_delegatesToNative() {
        when(mTrackerJniMock.wouldTriggerHelpUi(NATIVE_PTR, TEST_FEATURE)).thenReturn(false);
        when(mTrackerJniMock.shouldTriggerHelpUi(NATIVE_PTR, TEST_FEATURE)).thenReturn(false);
        mTracker.setEnterprisePolicyBlocksPromotionsSupplier(() -> true);

        FeatureProperties props = new FeatureProperties(UiFormat.DIALOG);
        assertFalse(mTracker.shouldTriggerHelpUi(TEST_FEATURE, props));

        // Should delegate to native to record normal failure metrics, not enterprise policy.
        verify(mTrackerJniMock).shouldTriggerHelpUi(NATIVE_PTR, TEST_FEATURE);
        assertEquals(
                0,
                mUserActionTester.getActionCount(
                        "InProductHelp.ShouldTriggerHelpUI." + TEST_FEATURE));
    }

    @Test
    public void testBlockedByEnterprisePolicy() {
        mTracker.setEnterprisePolicyBlocksPromotionsSupplier(() -> true);
        FeatureProperties props = new FeatureProperties(UiFormat.DIALOG);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "InProductHelp.ShouldTriggerHelpUI." + TEST_FEATURE,
                                TriggerHelpUIResult.FAILURE,
                                TriggerHelpUIResult.FAILURE_BLOCKED_BY_ENTERPRISE_POLICY)
                        .expectIntRecords(
                                "InProductHelp.ShouldTriggerHelpUI",
                                TriggerHelpUIResult.FAILURE,
                                TriggerHelpUIResult.FAILURE_BLOCKED_BY_ENTERPRISE_POLICY)
                        .build();

        assertFalse(mTracker.shouldTriggerHelpUi(TEST_FEATURE, props));

        watcher.assertExpected();
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "InProductHelp.ShouldTriggerHelpUI." + TEST_FEATURE));
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "InProductHelp.ShouldTriggerHelpUIResult.NotTriggered." + TEST_FEATURE));
        verify(mTrackerJniMock, never()).shouldTriggerHelpUi(NATIVE_PTR, TEST_FEATURE);
    }

    @Test
    public void testBypassEnterprisePolicy_allowlistedFeature() {
        String allowlistedFeature = FeatureConstants.NOTIFICATION_PERMISSION_PROMO_FEATURE;
        mTracker.setEnterprisePolicyBlocksPromotionsSupplier(() -> true);
        FeatureProperties props = new FeatureProperties(UiFormat.DIALOG);

        assertTrue(mTracker.shouldTriggerHelpUi(allowlistedFeature, props));
        verify(mTrackerJniMock).shouldTriggerHelpUi(NATIVE_PTR, allowlistedFeature);
    }

    @Test
    public void testWouldTriggerHelpUi_blockedByEnterprisePolicy() {
        mTracker.setEnterprisePolicyBlocksPromotionsSupplier(() -> true);

        assertFalse(mTracker.wouldTriggerHelpUi(TEST_FEATURE));
        verify(mTrackerJniMock, never()).wouldTriggerHelpUi(NATIVE_PTR, TEST_FEATURE);
    }

    @Test
    public void testWouldTriggerHelpUi_bypassEnterprisePolicy_allowlistedFeature() {
        String allowlistedFeature = FeatureConstants.NOTIFICATION_PERMISSION_PROMO_FEATURE;
        mTracker.setEnterprisePolicyBlocksPromotionsSupplier(() -> true);

        assertTrue(mTracker.wouldTriggerHelpUi(allowlistedFeature));
        verify(mTrackerJniMock).wouldTriggerHelpUi(NATIVE_PTR, allowlistedFeature);
    }

    @Test
    public void testBlockedByCooldownGroup() {
        when(mTrackerJniMock.wouldTriggerHelpUi(
                        NATIVE_PTR, FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP))
                .thenReturn(false);

        FeatureProperties props = new FeatureProperties(UiFormat.BOTTOM_SHEET);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "InProductHelp.ShouldTriggerHelpUI." + TEST_FEATURE,
                                TriggerHelpUIResult.FAILURE,
                                TriggerHelpUIResult.FAILURE_OTHER_PRECONDITION_UNMET)
                        .expectIntRecords(
                                "InProductHelp.ShouldTriggerHelpUI",
                                TriggerHelpUIResult.FAILURE,
                                TriggerHelpUIResult.FAILURE_OTHER_PRECONDITION_UNMET)
                        .build();

        assertFalse(mTracker.shouldTriggerHelpUi(TEST_FEATURE, props));

        watcher.assertExpected();
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "InProductHelp.CooldownGroupActive."
                                + FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP));
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "InProductHelp.ShouldTriggerHelpUI." + TEST_FEATURE));
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "InProductHelp.ShouldTriggerHelpUIResult.NotTriggered." + TEST_FEATURE));
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "InProductHelp.ShouldTriggerHelpUIResult.BlockedByCooldownGroup."
                                + TEST_FEATURE));
        verify(mTrackerJniMock, never()).shouldTriggerHelpUi(NATIVE_PTR, TEST_FEATURE);
    }

    @Test
    public void testBypassCooldownGroup_andBlocksCooldownGroup() {
        String allowlistedFeature = FeatureConstants.RESTORE_TABS_ON_FRE_FEATURE;
        when(mTrackerJniMock.wouldTriggerHelpUi(
                        NATIVE_PTR, FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP))
                .thenReturn(false);
        when(mTrackerJniMock.shouldTriggerHelpUi(
                        NATIVE_PTR, FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP))
                .thenReturn(false);

        FeatureProperties props = new FeatureProperties(UiFormat.BOTTOM_SHEET);

        assertTrue(mTracker.shouldTriggerHelpUi(allowlistedFeature, props));

        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "InProductHelp.CooldownGroupActive."
                                + FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP));
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "InProductHelp.ShouldTriggerHelpUIResult.BypassedCooldownGroup."
                                + allowlistedFeature));
        verify(mTrackerJniMock)
                .notifyEvent(NATIVE_PTR, EventConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP_TRIGGER);
        verify(mTrackerJniMock).shouldTriggerHelpUi(NATIVE_PTR, allowlistedFeature);
    }

    @Test
    public void testBlocksCooldownGroup_triggersAndDismissesCooldownFeature() {
        FeatureProperties props = new FeatureProperties(UiFormat.BLUE_BUBBLE);

        assertTrue(mTracker.shouldTriggerHelpUi(TEST_FEATURE, props));

        verify(mTrackerJniMock)
                .shouldTriggerHelpUi(NATIVE_PTR, FeatureConstants.BLUE_BUBBLE_COOLDOWN_GROUP);
        verify(mTrackerJniMock).dismissed(NATIVE_PTR, FeatureConstants.BLUE_BUBBLE_COOLDOWN_GROUP);
        verify(mTrackerJniMock).shouldTriggerHelpUi(NATIVE_PTR, TEST_FEATURE);
    }

    @Test
    public void testIsAllowedToSkipBlockingCooldownGroup_defaultsToFalse() {
        // No feature is exempt from contributing to its cooldown group yet, including features
        // that may bypass being blocked by it.
        assertFalse(CooldownBypassAllowlist.isAllowedToSkipBlockingCooldownGroup(TEST_FEATURE));
        assertFalse(
                CooldownBypassAllowlist.isAllowedToSkipBlockingCooldownGroup(
                        FeatureConstants.RESTORE_TABS_ON_FRE_FEATURE));
    }

    @Test
    public void testNonCooldownFormat_skipsCooldownChecks() {
        FeatureProperties props = new FeatureProperties(UiFormat.MAGIC_STACK);

        assertTrue(mTracker.shouldTriggerHelpUi(TEST_FEATURE, props));

        verify(mTrackerJniMock, never())
                .wouldTriggerHelpUi(NATIVE_PTR, FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP);
        verify(mTrackerJniMock, never())
                .wouldTriggerHelpUi(NATIVE_PTR, FeatureConstants.BLUE_BUBBLE_COOLDOWN_GROUP);
        verify(mTrackerJniMock).shouldTriggerHelpUi(NATIVE_PTR, TEST_FEATURE);
    }

    @Test
    public void testShouldTriggerHelpUi_InterruptiveCooldownGroupDisabled_SkipsCooldownChecks() {
        mTracker.setInterruptiveCooldownGroupEnabledSupplier(
                SupplierUtils.alwaysFalseBooleanSupplier());
        FeatureProperties props = new FeatureProperties(UiFormat.DIALOG);

        assertTrue(mTracker.shouldTriggerHelpUi(TEST_FEATURE, props));

        verify(mTrackerJniMock, never())
                .wouldTriggerHelpUi(NATIVE_PTR, FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP);
        verify(mTrackerJniMock, never())
                .shouldTriggerHelpUi(NATIVE_PTR, FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP);
        verify(mTrackerJniMock, never())
                .dismissed(NATIVE_PTR, FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP);
        verify(mTrackerJniMock).shouldTriggerHelpUi(NATIVE_PTR, TEST_FEATURE);
    }

    @Test
    public void testShouldTriggerHelpUi_BlueBubbleCooldownGroupDisabled_SkipsCooldownChecks() {
        mTracker.setBlueBubbleCooldownGroupEnabledSupplier(
                SupplierUtils.alwaysFalseBooleanSupplier());
        FeatureProperties props = new FeatureProperties(UiFormat.BLUE_BUBBLE);

        assertTrue(mTracker.shouldTriggerHelpUi(TEST_FEATURE, props));

        verify(mTrackerJniMock, never())
                .wouldTriggerHelpUi(NATIVE_PTR, FeatureConstants.BLUE_BUBBLE_COOLDOWN_GROUP);
        verify(mTrackerJniMock, never())
                .shouldTriggerHelpUi(NATIVE_PTR, FeatureConstants.BLUE_BUBBLE_COOLDOWN_GROUP);
        verify(mTrackerJniMock, never())
                .dismissed(NATIVE_PTR, FeatureConstants.BLUE_BUBBLE_COOLDOWN_GROUP);
        verify(mTrackerJniMock).shouldTriggerHelpUi(NATIVE_PTR, TEST_FEATURE);
    }

    @Test
    @SuppressWarnings("deprecation")
    public void testDeprecatedShouldTriggerHelpUi_aggressivelyCooldownIphs() {
        mTracker.setAggressivelyCooldownIphsSupplier(() -> true);

        assertTrue(mTracker.shouldTriggerHelpUi(TEST_FEATURE));

        verify(mTrackerJniMock)
                .shouldTriggerHelpUi(NATIVE_PTR, FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP);
        verify(mTrackerJniMock)
                .dismissed(NATIVE_PTR, FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP);
        verify(mTrackerJniMock)
                .shouldTriggerHelpUi(NATIVE_PTR, FeatureConstants.BLUE_BUBBLE_COOLDOWN_GROUP);
        verify(mTrackerJniMock).dismissed(NATIVE_PTR, FeatureConstants.BLUE_BUBBLE_COOLDOWN_GROUP);
    }

    @Test
    public void testShouldTriggerHelpUiWithSnooze() {
        FeatureProperties props = new FeatureProperties(UiFormat.ALERTS);

        TriggerDetails details = mTracker.shouldTriggerHelpUiWithSnooze(TEST_FEATURE, props);
        assertTrue(details.shouldTriggerIph);
        assertTrue(details.shouldShowSnooze);
        verify(mTrackerJniMock).shouldTriggerHelpUiWithSnooze(NATIVE_PTR, TEST_FEATURE);
    }

    @Test
    public void testFeaturePropertiesToString() {
        FeatureProperties props = new FeatureProperties(UiFormat.BLUE_BUBBLE);
        assertEquals("FeatureProperties{format=BLUE_BUBBLE}", props.toString());
    }
}
