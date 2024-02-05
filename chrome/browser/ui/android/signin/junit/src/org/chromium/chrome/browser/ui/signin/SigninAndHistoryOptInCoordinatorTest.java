// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Promise;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.ref.WeakReference;

/** Unit tests for {@link SigninAndHistoryOptInCoordinator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
public class SigninAndHistoryOptInCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Profile mProfile;
    @Mock private CoreAccountInfo mCoreAccountInfo;
    @Mock private AccountManagerFacade mAccountManagerFacade;
    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;
    @Mock private SigninManager mSigninManagerMock;
    @Mock private SigninMetricsUtils.Natives mSigninMetricsUtilsNativeMock;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    @Mock private DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    @Mock private SigninAndHistoryOptInCoordinator.Delegate mDelegate;

    private final @SigninAccessPoint int mAccessPoint = SigninAccessPoint.NTP_SIGNED_OUT_ICON;
    private Activity mActivity;
    private SigninAndHistoryOptInCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        mJniMocker.mock(SigninMetricsUtilsJni.TEST_HOOKS, mSigninMetricsUtilsNativeMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        AccountManagerFacadeProvider.setInstanceForTests(mAccountManagerFacade);
        when(mAccountManagerFacade.getCoreAccountInfos()).thenReturn(new Promise<>());
        when(mIdentityServicesProviderMock.getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

        mCoordinator =
                new SigninAndHistoryOptInCoordinator(
                        mWindowAndroid,
                        mActivity,
                        mDelegate,
                        mDeviceLockActivityLauncher,
                        mProfile,
                        mModalDialogManagerSupplier,
                        mAccessPoint);
    }

    @Test
    @MediumTest
    public void testSignIn_signInComplete() {
        when(mModalDialogManagerSupplier.get()).thenReturn(mModalDialogManager);
        doAnswer(
                        invocation -> {
                            SigninManager.SignInCallback callback = invocation.getArgument(2);
                            callback.onSignInComplete();
                            return null;
                        })
                .when(mSigninManagerMock)
                .signin(eq(mCoreAccountInfo), anyInt(), any());
        mCoordinator.signIn(mCoreAccountInfo, error -> {});

        // Verify that the SigninManager starts sign-in then a dialog is shown for history opt-in.
        verify(mSigninManagerMock, times(1)).signin(eq(mCoreAccountInfo), eq(mAccessPoint), any());
        verify(mModalDialogManager, times(1))
                .showDialog(
                        any(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(ModalDialogManager.ModalDialogPriority.VERY_HIGH));
    }

    @Test
    @MediumTest
    public void testSignIn_signInNotAllowed() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        mCoordinator.signIn(mCoreAccountInfo, error -> {});

        // Verify that the SigninManager never start sign-in and no dialog is shown for history
        // opt-in.
        verify(mSigninManagerMock, never()).signin(any(CoreAccountInfo.class), anyInt(), any());
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());
    }

    @Test
    @MediumTest
    public void testSignIn_signInAborted() {
        doAnswer(
                        invocation -> {
                            SigninManager.SignInCallback callback = invocation.getArgument(2);
                            callback.onSignInAborted();
                            return null;
                        })
                .when(mSigninManagerMock)
                .signin(eq(mCoreAccountInfo), anyInt(), any());

        mCoordinator.signIn(mCoreAccountInfo, error -> {});

        // Verify that the SigninManager starts sign-in but no dialog is shown for history opt-in.
        verify(mSigninManagerMock, times(1)).signin(eq(mCoreAccountInfo), eq(mAccessPoint), any());
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());
    }
}
