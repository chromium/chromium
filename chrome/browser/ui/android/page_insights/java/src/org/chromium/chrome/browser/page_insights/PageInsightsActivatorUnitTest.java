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
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.sync.SyncService;

/**
 * Unit tests for {@link PageInsightsActivator.java}.
 */
@LooperMode(LooperMode.Mode.PAUSED)
@RunWith(BaseRobolectricTestRunner.class)
public class PageInsightsActivatorUnitTest {
    @Mock
    private Profile mProfile;

    @Mock
    private IdentityServicesProvider mIdentityServicesProvider;

    @Mock
    private SigninManager mSigninManager;

    @Mock
    private SyncService mSyncService;

    @Mock
    private PageInsightsSwaaChecker mSwaaChecker;

    @Mock
    private Runnable mActivateCallback;

    private PageInsightsActivator mActivator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getSigninManager(any())).thenReturn(mSigninManager);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        mActivator = PageInsightsActivator.getForProfile(mProfile);
        mActivator.setSwaaCheckerForTesting(mSwaaChecker);
    }

    @Test
    public void testActivateCallback() throws Exception {
        int token = mActivator.start(mActivateCallback);

        mActivator.onSignedOut();
        verify(mActivateCallback, never()).run();

        mActivator.onSignedIn();
        verify(mActivateCallback).run();
        clearInvocations(mActivateCallback);

        mActivator.syncStateChanged();
        verify(mActivateCallback, never()).run();

        when(mSyncService.isSyncingUnencryptedUrls()).thenReturn(true);
        mActivator.syncStateChanged();
        verify(mActivateCallback).run();
        clearInvocations(mActivateCallback);

        mActivator.stop(token);
    }

    @Test
    public void testInvalidateSwaaCacheUponProfileChange() throws Exception {
        var prefs = SharedPreferencesManager.getInstance();
        prefs.disableKeyCheckerForTesting();

        int token = mActivator.start(mActivateCallback);

        prefs.writeLong(ChromePreferenceKeys.SWAA_TIMESTAMP, 1000L);
        prefs.writeBoolean(ChromePreferenceKeys.SWAA_STATUS, true);
        assertTrue(prefs.contains(ChromePreferenceKeys.SWAA_TIMESTAMP));
        assertTrue(prefs.contains(ChromePreferenceKeys.SWAA_STATUS));

        Profile profile2 = Mockito.mock(Profile.class);
        PageInsightsActivator.getForProfile(profile2);

        assertFalse(prefs.contains(ChromePreferenceKeys.SWAA_TIMESTAMP));
        assertFalse(prefs.contains(ChromePreferenceKeys.SWAA_STATUS));

        mActivator.stop(token);
    }
}
