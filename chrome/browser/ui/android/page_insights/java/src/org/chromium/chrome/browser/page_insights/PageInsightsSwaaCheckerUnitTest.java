// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Unit tests for {@link PageInsightsSwaaChecker}.
 */
@LooperMode(LooperMode.Mode.PAUSED)
@RunWith(BaseRobolectricTestRunner.class)
public class PageInsightsSwaaCheckerUnitTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private PageInsightsSwaaChecker.Natives mPageInsightsSwaaCheckerJni;

    @Mock
    private Profile mProfile;

    @Mock
    private Runnable mActivateCallback;

    private SharedPreferencesManager mSharedPreferencesManager;

    private PageInsightsSwaaChecker mSwaaChecker;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(PageInsightsSwaaCheckerJni.TEST_HOOKS, mPageInsightsSwaaCheckerJni);
        mSharedPreferencesManager = SharedPreferencesManager.getInstance();
        mSharedPreferencesManager.disableKeyCheckerForTesting();

        mSwaaChecker = new PageInsightsSwaaChecker(mProfile, mActivateCallback);
    }

    @After
    public void tearDown() {
        mSharedPreferencesManager.removeKey(ChromePreferenceKeys.SWAA_TIMESTAMP);
        mSharedPreferencesManager.removeKey(ChromePreferenceKeys.SWAA_STATUS);
    }

    @Test
    public void testIsEnabled() throws Exception {
        assertFalse(mSwaaChecker.isSwaaEnabled().isPresent());

        mSwaaChecker.onSwaaResponse(false);
        assertFalse(mSwaaChecker.isSwaaEnabled().get());

        mSwaaChecker.onSwaaResponse(true);
        assertTrue(mSwaaChecker.isSwaaEnabled().get());
    }

    @Test
    public void testEmptyOnExpiry() throws Exception {
        long timeNow = 1000L;
        mSwaaChecker.setElapsedRealtimeSupplierForTesting(() -> timeNow);
        mSwaaChecker.start();
        mSwaaChecker.onSwaaResponse(true);
        assertTrue(mSwaaChecker.isSwaaEnabled().get());

        // Returns the cached value
        long timeNow2 = timeNow + PageInsightsSwaaChecker.REFRESH_PERIOD_MS / 2;
        mSwaaChecker.setElapsedRealtimeSupplierForTesting(() -> timeNow2);
        assertTrue(mSwaaChecker.isSwaaEnabled().get());

        // Cache expired.
        long timeNow3 = timeNow + PageInsightsSwaaChecker.REFRESH_PERIOD_MS + 1;
        mSwaaChecker.setElapsedRealtimeSupplierForTesting(() -> timeNow3);
        assertFalse(mSwaaChecker.isSwaaEnabled().isPresent());
    }

    @Test
    public void testInvokeActivate() throws Exception {
        mSwaaChecker.onSwaaResponse(false);
        verify(mActivateCallback, never()).run();
        mSwaaChecker.onSwaaResponse(true);
        verify(mActivateCallback).run();
    }

    @Test
    public void testMultipleInstancesQueryAndUpdate() throws Exception {
        long timeNow = 1000L;
        mSwaaChecker.setElapsedRealtimeSupplierForTesting(() -> timeNow);
        mSwaaChecker.start();

        // Send a query when there is no cached data.
        verify(mPageInsightsSwaaCheckerJni).queryStatus(mSwaaChecker, mProfile);
        mSwaaChecker.onSwaaResponse(true);
        assertTrue(mSwaaChecker.isSwaaEnabled().get());
        mSwaaChecker.onSwaaResponse(false);
        assertFalse(mSwaaChecker.isSwaaEnabled().get());
        assertTrue(mSwaaChecker.isUpdateScheduled());
        clearInvocations(mPageInsightsSwaaCheckerJni);

        // The 2nd CCT instance picks up the cached value without sending a new query
        // if the cache is valid. Verify the handler has an update scheduled.
        long timeNow2 = timeNow + PageInsightsSwaaChecker.REFRESH_PERIOD_MS / 2;
        mSwaaChecker.setElapsedRealtimeSupplierForTesting(() -> timeNow2);
        mSwaaChecker.start();
        verify(mPageInsightsSwaaCheckerJni, never()).queryStatus(any(), any());
        assertTrue(mSwaaChecker.isUpdateScheduled());
    }

    @Test
    public void testReactToSignIn() throws Exception {
        mSwaaChecker.start();
        verify(mPageInsightsSwaaCheckerJni).queryStatus(any(), any());
        mSwaaChecker.onSwaaResponse(false);
        assertFalse(mSwaaChecker.isSwaaEnabled().get());
        clearInvocations(mPageInsightsSwaaCheckerJni);

        // On signing out, cache is invalidated.
        mSwaaChecker.onSignedOut();
        assertFalse(mSwaaChecker.isSwaaEnabled().isPresent());

        // On signing in, a new query is made to update the cache immediately.
        mSwaaChecker.onSignedIn();
        verify(mPageInsightsSwaaCheckerJni).queryStatus(any(), any());
    }
}
