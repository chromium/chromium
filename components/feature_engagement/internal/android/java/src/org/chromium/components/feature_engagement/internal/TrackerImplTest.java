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
        mTracker.setPromotionsEnabledSupplier(() -> false);

        FeatureProperties props = new FeatureProperties(UiFormat.DIALOG, true, true, true);
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
        mTracker.setPromotionsEnabledSupplier(() -> false);
        FeatureProperties props =
                new FeatureProperties(
                        UiFormat.DIALOG,
                        /* isBlockedByEnterprisePolicy= */ true,
                        /* isBlockedByCooldownGroup= */ true,
                        /* blocksCooldownGroup= */ true);

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
    public void testBypassEnterprisePolicy() {
        mTracker.setPromotionsEnabledSupplier(() -> false);
        FeatureProperties props =
                new FeatureProperties(
                        UiFormat.DIALOG,
                        /* isBlockedByEnterprisePolicy= */ false,
                        /* isBlockedByCooldownGroup= */ true,
                        /* blocksCooldownGroup= */ false);

        assertTrue(mTracker.shouldTriggerHelpUi(TEST_FEATURE, props));
        verify(mTrackerJniMock).shouldTriggerHelpUi(NATIVE_PTR, TEST_FEATURE);
    }

    @Test
    public void testBlockedByCooldownGroup() {
        when(mTrackerJniMock.wouldTriggerHelpUi(
                        NATIVE_PTR, FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP))
                .thenReturn(false);

        FeatureProperties props =
                new FeatureProperties(
                        UiFormat.BOTTOM_SHEET,
                        /* isBlockedByEnterprisePolicy= */ true,
                        /* isBlockedByCooldownGroup= */ true,
                        /* blocksCooldownGroup= */ true);

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
        when(mTrackerJniMock.wouldTriggerHelpUi(
                        NATIVE_PTR, FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP))
                .thenReturn(false);
        when(mTrackerJniMock.shouldTriggerHelpUi(
                        NATIVE_PTR, FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP))
                .thenReturn(false);

        FeatureProperties props =
                new FeatureProperties(
                        UiFormat.FRE,
                        /* isBlockedByEnterprisePolicy= */ true,
                        /* isBlockedByCooldownGroup= */ false,
                        /* blocksCooldownGroup= */ true);

        assertTrue(mTracker.shouldTriggerHelpUi(TEST_FEATURE, props));

        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "InProductHelp.CooldownGroupActive."
                                + FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP));
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "InProductHelp.ShouldTriggerHelpUIResult.BypassedCooldownGroup."
                                + TEST_FEATURE));
        verify(mTrackerJniMock)
                .notifyEvent(NATIVE_PTR, EventConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP_TRIGGER);
        verify(mTrackerJniMock).shouldTriggerHelpUi(NATIVE_PTR, TEST_FEATURE);
    }

    @Test
    public void testBlocksCooldownGroup_triggersAndDismissesCooldownFeature() {
        FeatureProperties props =
                new FeatureProperties(
                        UiFormat.BLUE_BUBBLE,
                        /* isBlockedByEnterprisePolicy= */ true,
                        /* isBlockedByCooldownGroup= */ true,
                        /* blocksCooldownGroup= */ true);

        assertTrue(mTracker.shouldTriggerHelpUi(TEST_FEATURE, props));

        verify(mTrackerJniMock)
                .shouldTriggerHelpUi(NATIVE_PTR, FeatureConstants.BLUE_BUBBLE_COOLDOWN_GROUP);
        verify(mTrackerJniMock).dismissed(NATIVE_PTR, FeatureConstants.BLUE_BUBBLE_COOLDOWN_GROUP);
        verify(mTrackerJniMock).shouldTriggerHelpUi(NATIVE_PTR, TEST_FEATURE);
    }

    @Test
    public void testNonCooldownFormat_skipsCooldownChecks() {
        FeatureProperties props =
                new FeatureProperties(
                        UiFormat.MAGIC_STACK,
                        /* isBlockedByEnterprisePolicy= */ true,
                        /* isBlockedByCooldownGroup= */ true,
                        /* blocksCooldownGroup= */ true);

        assertTrue(mTracker.shouldTriggerHelpUi(TEST_FEATURE, props));

        verify(mTrackerJniMock, never())
                .wouldTriggerHelpUi(NATIVE_PTR, FeatureConstants.INTERRUPTIVE_IPH_COOLDOWN_GROUP);
        verify(mTrackerJniMock, never())
                .wouldTriggerHelpUi(NATIVE_PTR, FeatureConstants.BLUE_BUBBLE_COOLDOWN_GROUP);
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
        FeatureProperties props =
                new FeatureProperties(
                        UiFormat.ALERTS,
                        /* isBlockedByEnterprisePolicy= */ true,
                        /* isBlockedByCooldownGroup= */ true,
                        /* blocksCooldownGroup= */ true);

        TriggerDetails details = mTracker.shouldTriggerHelpUiWithSnooze(TEST_FEATURE, props);
        assertTrue(details.shouldTriggerIph);
        assertTrue(details.shouldShowSnooze);
        verify(mTrackerJniMock).shouldTriggerHelpUiWithSnooze(NATIVE_PTR, TEST_FEATURE);
    }

    @Test
    public void testFeaturePropertiesToString() {
        FeatureProperties props =
                new FeatureProperties(
                        UiFormat.BLUE_BUBBLE,
                        /* isBlockedByEnterprisePolicy= */ true,
                        /* isBlockedByCooldownGroup= */ false,
                        /* blocksCooldownGroup= */ true);
        assertEquals(
                "FeatureProperties{format=BLUE_BUBBLE, isBlockedByEnterprisePolicy=true,"
                        + " isBlockedByCooldownGroup=false, blocksCooldownGroup=true}",
                props.toString());
    }
}
