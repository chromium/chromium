// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.ui.base.WindowAndroid;

/** Tests for {@link AutocompleteMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowLog.class, ShadowLooper.class})
public class AutocompleteControllerProviderUnitTest {
    private static final long NATIVE_CONTROLLER_1 = 111;
    private static final long NATIVE_CONTROLLER_2 = 222;

    public @Rule JniMocker mJniMocker = new JniMocker();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock Profile mProfile1;
    private @Mock Profile mProfile2;
    private @Mock AutocompleteController.Natives mAutocompleteControllerJniMock;

    private WindowAndroid mWindowAndroid1;
    private WindowAndroid mWindowAndroid2;
    private AutocompleteControllerProvider mProvider1;
    private AutocompleteControllerProvider mProvider2;
    private ShadowLooper mShadowLooper;

    @Before
    public void setUp() {
        mJniMocker.mock(AutocompleteControllerJni.TEST_HOOKS, mAutocompleteControllerJniMock);
        doReturn(NATIVE_CONTROLLER_1)
                .when(mAutocompleteControllerJniMock)
                .create(any(), eq(mProfile1));
        doReturn(NATIVE_CONTROLLER_2)
                .when(mAutocompleteControllerJniMock)
                .create(any(), eq(mProfile2));

        mShadowLooper = ShadowLooper.shadowMainLooper();
        mWindowAndroid1 = new WindowAndroid(ContextUtils.getApplicationContext());
        mWindowAndroid2 = new WindowAndroid(ContextUtils.getApplicationContext());
        mProvider1 = AutocompleteControllerProvider.from(mWindowAndroid1);
        mProvider2 = AutocompleteControllerProvider.from(mWindowAndroid2);
    }

    @After
    public void tearDown() {
        mWindowAndroid1.destroy();
        mWindowAndroid2.destroy();
        mShadowLooper.runToEndOfTasks();
    }

    @Test
    @SmallTest
    public void multiWindow_providersAreNotSharedAcrossWindows() {
        assertNotEquals(mProvider1, mProvider2);
    }

    @Test
    @SmallTest
    public void singleWindow_providersAreSharedWithinWindow() {
        var provider = AutocompleteControllerProvider.from(mWindowAndroid1);
        assertEquals(mProvider1, provider);
    }

    @Test
    @SmallTest
    public void multiWindow_controllersAreNotSharedAcrossWindows() {
        var controller1 = mProvider1.get(mProfile1);
        var controller2 = mProvider2.get(mProfile1);
        assertNotEquals(controller1, controller2);
    }

    @Test
    @SmallTest
    public void singleWindow_controllersAreSharedWithinWindow() {
        var controller1 = mProvider1.get(mProfile1);
        var controller2 = mProvider1.get(mProfile1);
        assertEquals(controller1, controller2);
    }

    @Test
    @SmallTest
    public void controllersAreNotSharedAcrossProfiles() {
        var controller1 = mProvider1.get(mProfile1);
        var controller2 = mProvider1.get(mProfile2);
        assertNotEquals(controller1, controller2);
    }

    @Test
    @SmallTest
    public void controllersAreDestroyedWithProfile() {
        var controller1 = mProvider1.get(mProfile1);
        var controller2 = mProvider1.get(mProfile2);

        reset(mAutocompleteControllerJniMock);

        ProfileManager.onProfileDestroyed(mProfile2);
        verify(mAutocompleteControllerJniMock, times(1)).destroy(eq(NATIVE_CONTROLLER_2));
        ProfileManager.onProfileDestroyed(mProfile1);
        verify(mAutocompleteControllerJniMock, times(1)).destroy(eq(NATIVE_CONTROLLER_1));
        verifyNoMoreInteractions(mAutocompleteControllerJniMock);
    }

    @Test
    @SmallTest
    public void controllersAreDestroyedWithProvider() {
        var controller1 = mProvider1.get(mProfile1);
        var controller2 = mProvider1.get(mProfile2);

        reset(mAutocompleteControllerJniMock);
        mProvider1.onDetachedFromHost(mWindowAndroid1.getUnownedUserDataHost());
        verify(mAutocompleteControllerJniMock, times(1)).destroy(eq(NATIVE_CONTROLLER_2));
        verify(mAutocompleteControllerJniMock, times(1)).destroy(eq(NATIVE_CONTROLLER_1));
        verifyNoMoreInteractions(mAutocompleteControllerJniMock);
    }

    @Test
    @SmallTest
    public void singleWindow_controllersAreDestroyedOnlyOnce() {
        var controller1 = mProvider1.get(mProfile1);
        var controller2 = mProvider1.get(mProfile2);
        var controller3 = mProvider1.get(mProfile2);

        reset(mAutocompleteControllerJniMock);

        ProfileManager.onProfileDestroyed(mProfile1);
        ProfileManager.onProfileDestroyed(mProfile2);
        verify(mAutocompleteControllerJniMock, times(1)).destroy(eq(NATIVE_CONTROLLER_1));
        verify(mAutocompleteControllerJniMock, times(1)).destroy(eq(NATIVE_CONTROLLER_2));
        verifyNoMoreInteractions(mAutocompleteControllerJniMock);

        // If, for any reason, ProfileManager passed on information that the profile has been
        // destroyed more than once, we should not take any action.
        ProfileManager.onProfileDestroyed(mProfile1);
        ProfileManager.onProfileDestroyed(mProfile2);
        verifyNoMoreInteractions(mAutocompleteControllerJniMock);

        mProvider1.onDetachedFromHost(mWindowAndroid1.getUnownedUserDataHost());
        verifyNoMoreInteractions(mAutocompleteControllerJniMock);
    }

    @Test
    @SmallTest
    public void multiWindow_controllersAreDestroyedForAppropriateWindow() {
        var controller1 = mProvider1.get(mProfile1);
        var controller2 = mProvider2.get(mProfile1);

        // Two distinct controllers are associated with the same profile.
        verify(mAutocompleteControllerJniMock, times(2)).create(any(), eq(mProfile1));

        // Dispose of all the controllers associated with the first window.
        mProvider1.onDetachedFromHost(mWindowAndroid1.getUnownedUserDataHost());
        verify(mAutocompleteControllerJniMock, times(1)).destroy(eq(NATIVE_CONTROLLER_1));
        verifyNoMoreInteractions(mAutocompleteControllerJniMock);

        clearInvocations(mAutocompleteControllerJniMock);

        // The other controller should be destroyed separately.
        mProvider2.onDetachedFromHost(mWindowAndroid2.getUnownedUserDataHost());
        verify(mAutocompleteControllerJniMock, times(1)).destroy(eq(NATIVE_CONTROLLER_1));
        verifyNoMoreInteractions(mAutocompleteControllerJniMock);
    }

    @Test
    @SmallTest
    public void multiWindow_controllersAreDestroyedForAppropriateProfile() {
        var controller1 = mProvider1.get(mProfile1);
        var controller2 = mProvider2.get(mProfile1);

        // Two distinct controllers are associated with the same profile.
        verify(mAutocompleteControllerJniMock, times(2)).create(any(), eq(mProfile1));

        // Controllers should be removed everywhere if the profile is gone.
        ProfileManager.onProfileDestroyed(mProfile1);
        verify(mAutocompleteControllerJniMock, times(2)).destroy(NATIVE_CONTROLLER_1);
        verifyNoMoreInteractions(mAutocompleteControllerJniMock);

        mProvider1.onDetachedFromHost(mWindowAndroid1.getUnownedUserDataHost());
        mProvider2.onDetachedFromHost(mWindowAndroid2.getUnownedUserDataHost());
        verifyNoMoreInteractions(mAutocompleteControllerJniMock);
    }

    @Test
    @SmallTest
    public void closeableControllers_controllersAreNotReused() {
        // Note: there is no strong indication that this should not be allowed.
        // At the time this is conceived, the only consumer of CloseableControllers is the
        // IntentHandler, so there's no reason for us to cache the one-shot controllers.
        var controller1 = AutocompleteControllerProvider.createCloseableController(mProfile1);
        var controller2 = AutocompleteControllerProvider.createCloseableController(mProfile1);
        assertNotEquals(controller1.get(), controller2.get());
        controller1.close();
        controller2.close();
    }

    @Test
    @SmallTest
    public void closeableControllers_controllersAreNotCreatedUponAccess() {
        try (var controller = AutocompleteControllerProvider.createCloseableController(mProfile1)) {
            assertEquals(controller.get(), controller.get());
        }
    }

    @Test
    @SmallTest
    public void closeableControllers_releasedWhenOutOfScope() {
        try (var controller = AutocompleteControllerProvider.createCloseableController(mProfile2)) {
            verify(mAutocompleteControllerJniMock, times(1)).create(any(), eq(mProfile2));
            verifyNoMoreInteractions(mAutocompleteControllerJniMock);
        }
        verify(mAutocompleteControllerJniMock, times(1)).destroy(NATIVE_CONTROLLER_2);
        verifyNoMoreInteractions(mAutocompleteControllerJniMock);
    }

    @Test
    @SmallTest
    public void controllersAreNotAutomaticallyCreatedWhenProfilesAreAdded() {
        ProfileManager.onProfileAdded(mProfile1);
        verifyNoMoreInteractions(mAutocompleteControllerJniMock);
        ProfileManager.resetForTesting();
    }
}
