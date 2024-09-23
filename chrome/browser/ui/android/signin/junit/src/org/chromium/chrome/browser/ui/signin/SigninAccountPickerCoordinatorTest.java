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

import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.activity.ComponentActivity;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Promise;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetMediator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerLaunchMode;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link SigninAccountPickerCoordinator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowPostTask.class})
@EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
public class SigninAccountPickerCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Profile mProfileMock;
    @Mock private CoreAccountInfo mCoreAccountInfoMock;
    @Mock private AccountManagerFacade mAccountManagerFacadeMock;
    @Mock private SigninManager mSigninManagerMock;
    @Mock private SigninMetricsUtils.Natives mSigninMetricsUtilsNativeMock;
    @Mock private WindowAndroid mWindowAndroidMock;
    @Mock private DeviceLockActivityLauncher mDeviceLockActivityLauncherMock;
    @Mock private SigninAccountPickerCoordinator.Delegate mDelegateMock;
    @Mock private AccountPickerBottomSheetMediator mMediator;

    private final @SigninAccessPoint int mAccessPoint = SigninAccessPoint.NTP_SIGNED_OUT_ICON;
    private ComponentActivity mActivity;
    private ViewGroup mContainerView;
    private SigninAccountPickerCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mActivity = activity;
                            mContainerView = new FrameLayout(mActivity);
                            mActivity.setContentView(mContainerView);
                        });
        ShadowPostTask.setTestImpl((taskTraits, task, delay) -> task.run());
        mJniMocker.mock(SigninMetricsUtilsJni.TEST_HOOKS, mSigninMetricsUtilsNativeMock);
        AccountManagerFacadeProvider.setInstanceForTests(mAccountManagerFacadeMock);
        when(mAccountManagerFacadeMock.getCoreAccountInfos()).thenReturn(new Promise<>());
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(mActivity));
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(
                                R.string.signin_account_picker_bottom_sheet_title)
                        .build();
        mCoordinator =
                new SigninAccountPickerCoordinator(
                        mWindowAndroidMock,
                        mActivity,
                        mContainerView,
                        mDelegateMock,
                        mDeviceLockActivityLauncherMock,
                        mSigninManagerMock,
                        bottomSheetStrings,
                        AccountPickerLaunchMode.DEFAULT,
                        mAccessPoint);
    }

    @Test
    @MediumTest
    public void testSignIn_signInComplete() {
        doAnswer(
                        invocation -> {
                            SigninManager.SignInCallback callback = invocation.getArgument(2);
                            callback.onSignInComplete();
                            return null;
                        })
                .when(mSigninManagerMock)
                .signin(eq(mCoreAccountInfoMock), anyInt(), any());
        mCoordinator.signIn(mCoreAccountInfoMock, mMediator);

        // Verify that the SigninManager starts sign-in then a dialog is shown for history opt-in.
        verify(mSigninManagerMock, times(1))
                .signin(eq(mCoreAccountInfoMock), eq(mAccessPoint), any());
        verify(mDelegateMock, times(1)).onSignInComplete();
        verify(mDelegateMock, never()).onSignInCancel();
    }

    @Test
    @MediumTest
    public void testSignIn_signInNotAllowed() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        mCoordinator.signIn(mCoreAccountInfoMock, mMediator);

        // Verify that the SigninManager never start sign-in and no dialog is shown for history
        // opt-in.
        verify(mSigninManagerMock, never()).signin(any(CoreAccountInfo.class), anyInt(), any());
        verify(mDelegateMock, never()).onSignInComplete();
        verify(mDelegateMock, never()).onSignInCancel();
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
                .signin(eq(mCoreAccountInfoMock), anyInt(), any());

        mCoordinator.signIn(mCoreAccountInfoMock, mMediator);

        // Verify that the SigninManager starts sign-in but no dialog is shown for history opt-in.
        // TODO(crbug.com/41493768): Update the verification when the final error states will
        // be implemented, and add test to ensure that the delegate is called only once in the
        // coordinator's lifetime.
        verify(mSigninManagerMock, times(1))
                .signin(eq(mCoreAccountInfoMock), eq(mAccessPoint), any());
        verify(mDelegateMock, never()).onSignInComplete();
        verify(mDelegateMock, never()).onSignInCancel();
    }
}
