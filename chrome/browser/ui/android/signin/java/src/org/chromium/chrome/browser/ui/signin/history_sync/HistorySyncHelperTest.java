// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.time.Duration;
import java.util.Set;

/** Unit tests for the {@link HistorySyncHelper} */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class HistorySyncHelperTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private SyncService mSyncServiceMock;
    @Mock private Profile mProfileMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;

    private HistorySyncHelper mHistorySyncHelper;

    @Before
    public void setUp() {
        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(any(Profile.class))).thenReturn(mPrefServiceMock);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHistorySyncHelper = HistorySyncHelper.getForProfile(mProfileMock);
                });
    }

    @Test
    @SmallTest
    public void testDidAlreadyOptIn() {
        Assert.assertFalse(mHistorySyncHelper.shouldSuppressHistorySync());
        Assert.assertFalse(mHistorySyncHelper.didAlreadyOptIn());

        when(mSyncServiceMock.getSelectedTypes())
                .thenReturn(Set.of(UserSelectableType.HISTORY, UserSelectableType.TABS));

        Assert.assertTrue(mHistorySyncHelper.didAlreadyOptIn());
        Assert.assertTrue(mHistorySyncHelper.shouldSuppressHistorySync());
    }

    @Test
    @SmallTest
    public void testIsHistorySyncDisabledByPolicy_syncDisabledByPolicy() {
        Assert.assertFalse(mHistorySyncHelper.shouldSuppressHistorySync());
        Assert.assertFalse(mHistorySyncHelper.isHistorySyncDisabledByPolicy());

        when(mSyncServiceMock.isSyncDisabledByEnterprisePolicy()).thenReturn(true);

        Assert.assertTrue(mHistorySyncHelper.isHistorySyncDisabledByPolicy());
        Assert.assertTrue(mHistorySyncHelper.shouldSuppressHistorySync());
    }

    @Test
    @SmallTest
    public void testIsHistorySyncDisabledByPolicy_typesManagedByPolicy() {
        Assert.assertFalse(mHistorySyncHelper.isHistorySyncDisabledByPolicy());

        when(mSyncServiceMock.isTypeManagedByPolicy(anyInt())).thenReturn(true);

        Assert.assertTrue(mHistorySyncHelper.isHistorySyncDisabledByPolicy());
        Assert.assertTrue(mHistorySyncHelper.shouldSuppressHistorySync());
    }

    @Test
    @SmallTest
    public void testIsHistorySyncDisabledByCustodian() {
        Assert.assertFalse(mHistorySyncHelper.isHistorySyncDisabledByCustodian());

        when(mSyncServiceMock.isTypeManagedByCustodian(anyInt())).thenReturn(true);

        Assert.assertTrue(mHistorySyncHelper.isHistorySyncDisabledByCustodian());
        Assert.assertTrue(mHistorySyncHelper.shouldSuppressHistorySync());
    }

    @Test
    @SmallTest
    public void testIsDeclinedOften_didDeclineInThePastTwoWeeks() {
        when(mPrefServiceMock.getLong(Pref.HISTORY_SYNC_LAST_DECLINED_TIMESTAMP))
                .thenReturn(TimeUtils.currentTimeMillis());

        Assert.assertTrue(mHistorySyncHelper.isDeclinedOften());
    }

    @Test
    @SmallTest
    public void testIsDeclinedOften_didDeclineTwiceInARow() {
        when(mPrefServiceMock.getInteger(Pref.HISTORY_SYNC_SUCCESSIVE_DECLINE_COUNT)).thenReturn(2);

        Assert.assertTrue(mHistorySyncHelper.isDeclinedOften());
    }

    @Test
    @SmallTest
    public void testIsDeclinedOften() {
        when(mPrefServiceMock.getInteger(Pref.HISTORY_SYNC_SUCCESSIVE_DECLINE_COUNT)).thenReturn(0);
        when(mPrefServiceMock.getLong(Pref.HISTORY_SYNC_LAST_DECLINED_TIMESTAMP))
                .thenReturn(TimeUtils.currentTimeMillis());

        mFakeTimeTestRule.advanceMillis(Duration.ofDays(15).toMillis());

        Assert.assertFalse(mHistorySyncHelper.isDeclinedOften());
    }

    @Test
    @SmallTest
    public void testShouldSuppressHistorySync() {
        when(mSyncServiceMock.getSelectedTypes()).thenReturn(Set.of());
        when(mSyncServiceMock.isTypeManagedByCustodian(anyInt())).thenReturn(false);
        when(mSyncServiceMock.isTypeManagedByPolicy(anyInt())).thenReturn(false);
        when(mSyncServiceMock.isSyncDisabledByEnterprisePolicy()).thenReturn(false);

        Assert.assertFalse(mHistorySyncHelper.shouldSuppressHistorySync());
    }

    @Test
    @SmallTest
    public void testRecordHistorySyncDeclinedPrefs() {
        final int someIntegerValue =
                mPrefServiceMock.getInteger(Pref.HISTORY_SYNC_SUCCESSIVE_DECLINE_COUNT);

        mHistorySyncHelper.recordHistorySyncDeclinedPrefs();

        verify(mPrefServiceMock)
                .setInteger(Pref.HISTORY_SYNC_SUCCESSIVE_DECLINE_COUNT, someIntegerValue + 1);
        verify(mPrefServiceMock)
                .setLong(Pref.HISTORY_SYNC_LAST_DECLINED_TIMESTAMP, TimeUtils.currentTimeMillis());
    }

    @Test
    @SmallTest
    public void testClearHistorySyncDeclinedPrefs() {
        mHistorySyncHelper.clearHistorySyncDeclinedPrefs();

        verify(mPrefServiceMock).clearPref(Pref.HISTORY_SYNC_LAST_DECLINED_TIMESTAMP);
        verify(mPrefServiceMock).clearPref(Pref.HISTORY_SYNC_SUCCESSIVE_DECLINE_COUNT);
    }

    @Test
    @SmallTest
    public void testRecordHistorySyncNotShown_userAlreadyOptedIn() {
        when(mSyncServiceMock.getSelectedTypes())
                .thenReturn(Set.of(UserSelectableType.HISTORY, UserSelectableType.TABS));
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.HistorySyncOptIn.AlreadyOptedIn", SigninAccessPoint.UNKNOWN);

        mHistorySyncHelper.recordHistorySyncNotShown(SigninAccessPoint.UNKNOWN);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordHistorySyncNotShown_userNotOptedIn() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.HistorySyncOptIn.Skipped", SigninAccessPoint.UNKNOWN);

        mHistorySyncHelper.recordHistorySyncNotShown(SigninAccessPoint.UNKNOWN);

        histogramWatcher.assertExpected();
    }
}
