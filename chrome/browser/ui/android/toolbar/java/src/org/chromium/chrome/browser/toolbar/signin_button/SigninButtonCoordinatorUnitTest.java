// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit tests for {@link SigninButtonCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SigninButtonCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private SigninAndHistorySyncActivityLauncher mSigninAndHistorySyncActivityLauncher;
    @Mock private ActivityResultTracker mActivityResultTracker;
    @Mock private DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;
    @Mock private OmniboxStub mOmniboxStub;
    @Mock private Tab mTab;

    private final SettableNullableObservableSupplier<Tab> mTabSupplier =
            ObservableSuppliers.createNullable();
    private final SettableMonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();
    private final OneshotSupplierImpl<OmniboxStub> mOmniboxStubSupplier =
            new OneshotSupplierImpl<>();

    private boolean mIsNtp;
    private SigninButtonCoordinator mCoordinator;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mCoordinator =
                new SigninButtonCoordinator(
                        activity,
                        mWindowAndroid,
                        /* viewStub= */ null,
                        mTabSupplier,
                        /* isNtpSupplier= */ () -> mIsNtp,
                        mOmniboxStubSupplier,
                        /* onSigninTapped= */ () -> {},
                        /* transitionTrigger= */ () -> {},
                        mProfileSupplier,
                        mSigninAndHistorySyncActivityLauncher,
                        mActivityResultTracker,
                        mDeviceLockActivityLauncher,
                        mBottomSheetController,
                        mModalDialogManager,
                        mSnackbarManager,
                        mThemeColorProvider,
                        mIncognitoStateProvider);
        mTabSupplier.set(mTab);
    }

    @Test
    public void testFollowsNtpSupplier() {
        mIsNtp = true;
        mCoordinator.updateButtonVisibility();
        assertTrue(mCoordinator.isShown());

        mIsNtp = false;
        mCoordinator.updateButtonVisibility();
        assertFalse(mCoordinator.isShown());
    }

    @Test
    public void testHiddenInIncognito() {
        mIsNtp = true;
        when(mTab.isOffTheRecord()).thenReturn(true);

        mCoordinator.updateButtonVisibility();
        assertFalse(mCoordinator.isShown());
    }

    @Test
    public void testHiddenWhenUrlBarFocused() {
        mIsNtp = true;
        mOmniboxStubSupplier.set(mOmniboxStub);
        ShadowLooper.idleMainLooper();
        mCoordinator.updateButtonVisibility();
        assertTrue(mCoordinator.isShown());

        mCoordinator.onUrlFocusChange(true);
        assertFalse(mCoordinator.isShown());

        mCoordinator.onUrlFocusChange(false);
        assertTrue(mCoordinator.isShown());
    }

    @Test
    public void testShowOnAllPages_ignoresNtpSupplier() {
        mIsNtp = false;
        mCoordinator.setShowOnAllPages(true);

        mCoordinator.updateButtonVisibility();
        assertTrue(mCoordinator.isShown());
    }
}
