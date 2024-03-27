// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import static org.mockito.ArgumentMatchers.anyInt;
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

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Set;

/** Unit tests for the {@link HistorySyncHelper} */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class HistorySyncHelperTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private SyncService mSyncServiceMock;
    @Mock private Profile mProfileMock;

    private HistorySyncHelper mHistorySyncHelper;

    @Before
    public void setUp() {
        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHistorySyncHelper = HistorySyncHelper.getForProfile(mProfileMock);
                });
    }

    @Test
    @SmallTest
    public void testDidAlreadyOptIn() {
        Assert.assertFalse(mHistorySyncHelper.didAlreadyOptIn());

        when(mSyncServiceMock.getSelectedTypes())
                .thenReturn(Set.of(UserSelectableType.HISTORY, UserSelectableType.TABS));

        Assert.assertTrue(mHistorySyncHelper.didAlreadyOptIn());
    }

    @Test
    @SmallTest
    public void testIsHistorySyncDisabledByPolicy_syncDisabledByPolicy() {
        Assert.assertFalse(mHistorySyncHelper.isHistorySyncDisabledByPolicy());

        when(mSyncServiceMock.isSyncDisabledByEnterprisePolicy()).thenReturn(true);

        Assert.assertTrue(mHistorySyncHelper.isHistorySyncDisabledByPolicy());
    }

    @Test
    @SmallTest
    public void testIsHistorySyncDisabledByPolicy_typesManagedByPolicy() {
        Assert.assertFalse(mHistorySyncHelper.isHistorySyncDisabledByPolicy());

        when(mSyncServiceMock.isTypeManagedByPolicy(anyInt())).thenReturn(true);

        Assert.assertTrue(mHistorySyncHelper.isHistorySyncDisabledByPolicy());
    }

    @Test
    @SmallTest
    public void testIsHistorySyncDisabledByCustodian() {
        Assert.assertFalse(mHistorySyncHelper.isHistorySyncDisabledByCustodian());

        when(mSyncServiceMock.isTypeManagedByCustodian(anyInt())).thenReturn(true);

        Assert.assertTrue(mHistorySyncHelper.isHistorySyncDisabledByCustodian());
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
