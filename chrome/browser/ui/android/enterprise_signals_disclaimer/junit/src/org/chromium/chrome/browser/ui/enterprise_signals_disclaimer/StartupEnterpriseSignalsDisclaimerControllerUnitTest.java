// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.OngoingStubbing;
import org.mockito.verification.VerificationMode;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtilsJni;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerController.CoordinatorFactory;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerController.HostFactory;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerController.ShownOn;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerCoordinator.PresentationMode;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerHost.DismissalCause;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.signin.test.util.FakeIdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;

import java.util.function.Consumer;

/** Unit tests for {@link StartupEnterpriseSignalsDisclaimerController}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
public class StartupEnterpriseSignalsDisclaimerControllerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private SigninManager mSigninManager;
    @Mock private EnterpriseSignalsDisclaimerCoordinator mCoordinator;
    @Mock private CoordinatorFactory mCoordinatorFactory;
    @Mock private EnterpriseSignalsDisclaimerHost mHost;
    @Mock private HostFactory mHostFactory;
    @Mock private Callback<String> mShowInfoPageCallback;
    @Mock private ManagedBrowserUtils.Natives mManagedBrowserUtilsJniMock;
    @Mock private EnterpriseSignalsDisclaimerBridge.Natives mBridgeNativesMock;

    @Captor private ArgumentCaptor<Consumer<Integer>> mDismissalCallbackCaptor;

    private final FakeIdentityManager mIdentityManager = new FakeIdentityManager();
    private AppCompatActivity mActivity;

    @Before
    public void setUp() {
        ManagedBrowserUtilsJni.setInstanceForTesting(mManagedBrowserUtilsJniMock);
        EnterpriseSignalsDisclaimerBridgeJni.setInstanceForTesting(mBridgeNativesMock);
        IdentityServicesProvider.setSigninManagerForTesting(mSigninManager);

        mActivity = Robolectric.buildActivity(AppCompatActivity.class).get();

        // Shared defaults which are only needed by tests that try to show the disclaimer.
        lenient().when(mSigninManager.getIdentityManager()).thenReturn(mIdentityManager);
        lenient()
                .when(mCoordinatorFactory.create(any(), any(), any(), anyInt(), any()))
                .thenReturn(mCoordinator);
        lenient().when(mHostFactory.create(anyInt(), any(), any())).thenReturn(mHost);

        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);
    }

    @After
    public void tearDown() {
        ManagedBrowserUtilsJni.setInstanceForTesting(null);
        EnterpriseSignalsDisclaimerBridgeJni.setInstanceForTesting(null);
    }

    private StartupEnterpriseSignalsDisclaimerController createController() {
        return StartupEnterpriseSignalsDisclaimerController.maybeCreateForProfile(
                mProfile, mActivity, mShowInfoPageCallback, mCoordinatorFactory, mHostFactory);
    }

    private StartupEnterpriseSignalsDisclaimerController createManagedControllerAndShow() {
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);
        StartupEnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);
        Assert.assertTrue(controller.maybeShow(ShownOn.STARTUP));
        return controller;
    }

    private OngoingStubbing<EnterpriseSignalsDisclaimerCoordinator> whenCoordinatorCreated() {
        return when(mCoordinatorFactory.create(any(), any(), any(), anyInt(), any()));
    }

    private OngoingStubbing<EnterpriseSignalsDisclaimerHost> whenHostCreated() {
        return when(mHostFactory.create(anyInt(), any(), any()));
    }

    private void verifyCoordinatorCreated(VerificationMode mode) {
        verify(mCoordinatorFactory, mode).create(any(), any(), any(), anyInt(), any());
    }

    private void verifyHostCreated(VerificationMode mode) {
        verify(mHostFactory, mode).create(anyInt(), any(), any());
    }

    /** Returns the delegate passed to the most recently created coordinator. */
    private EnterpriseSignalsDisclaimerCoordinator.Delegate captureDelegate() {
        ArgumentCaptor<EnterpriseSignalsDisclaimerCoordinator.Delegate> captor =
                ArgumentCaptor.forClass(EnterpriseSignalsDisclaimerCoordinator.Delegate.class);
        verify(mCoordinatorFactory, atLeastOnce())
                .create(any(), any(), any(), anyInt(), captor.capture());
        return captor.getValue();
    }

    /** Simulates the most recently created host reporting that it was dismissed. */
    private void simulateHostDismissed(@DismissalCause int dismissalCause) {
        verify(mHostFactory, atLeastOnce())
                .create(anyInt(), any(), mDismissalCallbackCaptor.capture());
        mDismissalCallbackCaptor.getValue().accept(dismissalCause);
    }

    private void runSignOutOperationsSynchronously() {
        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mSigninManager)
                .runAfterOperationInProgress(any());
    }

    @Test
    public void maybeCreateForProfile_offTheRecordProfile_returnsNull() {
        when(mProfile.isOffTheRecord()).thenReturn(true);

        StartupEnterpriseSignalsDisclaimerController controller = createController();

        Assert.assertNull(controller);
    }

    @Test
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void maybeCreateForProfile_hasDisableFirstRunExperienceSwitch_returnsNull() {
        StartupEnterpriseSignalsDisclaimerController controller = createController();

        Assert.assertNull(controller);
    }

    @Test
    public void maybeCreateForProfile_validProfile_returnsInstance() {
        StartupEnterpriseSignalsDisclaimerController controller = createController();

        Assert.assertNotNull(controller);
    }

    @Test
    public void maybeCreateForProfile_validProfileNoPrimaryAccount_returnsInstance() {
        mIdentityManager.setPrimaryAccount(null);

        StartupEnterpriseSignalsDisclaimerController controller = createController();

        Assert.assertNotNull(controller);
    }

    @Test
    public void maybeShow_destroyed_returnsFalse() {
        StartupEnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        controller.destroy();

        Assert.assertFalse(controller.maybeShow(ShownOn.STARTUP));
        verifyCoordinatorCreated(never());
        verifyHostCreated(never());
    }

    @Test
    public void maybeShow_noPrimaryAccount_returnsFalse() {
        StartupEnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        mIdentityManager.setPrimaryAccount(null);

        Assert.assertFalse(controller.maybeShow(ShownOn.STARTUP));
        verifyCoordinatorCreated(never());
        verifyHostCreated(never());
    }

    @Test
    public void maybeShow_profileNotManaged_returnsFalse() {
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(false);

        StartupEnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        Assert.assertFalse(controller.maybeShow(ShownOn.STARTUP));
        verifyCoordinatorCreated(never());
        verifyHostCreated(never());
    }

    @Test
    public void maybeShow_accountIsManaged_showsHostAndReturnsTrue() {
        createManagedControllerAndShow();

        verify(mBridgeNativesMock)
                .hasAccountAcknowledgedSignalsDisclaimer(
                        eq(mProfile), eq(TestAccounts.ACCOUNT1.getGaiaId()));
        verify(mCoordinatorFactory)
                .create(
                        eq(mActivity),
                        eq(mIdentityManager),
                        eq(TestAccounts.ACCOUNT1),
                        anyInt(),
                        any());
        verify(mHostFactory).create(anyInt(), eq(mCoordinator), any());
        verify(mHost).show();
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void maybeShow_accountIsManagedOnPhone_usesBottomSheetPresentationMode() {
        createManagedControllerAndShow();

        verify(mCoordinatorFactory)
                .create(any(), any(), any(), eq(PresentationMode.BOTTOM_SHEET), any());
        verify(mHostFactory).create(eq(PresentationMode.BOTTOM_SHEET), any(), any());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void maybeShow_accountIsManagedOnTablet_usesModalDialogPresentationMode() {
        createManagedControllerAndShow();

        verify(mCoordinatorFactory)
                .create(any(), any(), any(), eq(PresentationMode.MODAL_DIALOG), any());
        verify(mHostFactory).create(eq(PresentationMode.MODAL_DIALOG), any(), any());
    }

    @Test
    public void maybeShow_accountAlreadyAcknowledged_returnsFalse() {
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);
        when(mBridgeNativesMock.hasAccountAcknowledgedSignalsDisclaimer(
                        eq(mProfile), eq(TestAccounts.ACCOUNT1.getGaiaId())))
                .thenReturn(true);

        StartupEnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        Assert.assertFalse(controller.maybeShow(ShownOn.STARTUP));
        verifyCoordinatorCreated(never());
        verifyHostCreated(never());
    }

    @Test
    public void maybeShow_alreadyShowing_returnsFalse() {
        when(mHost.isActive()).thenReturn(true);
        StartupEnterpriseSignalsDisclaimerController controller = createManagedControllerAndShow();

        Assert.assertFalse(controller.maybeShow(ShownOn.STARTUP));

        verifyCoordinatorCreated(times(1));
        verifyHostCreated(times(1));
        verify(mHost, times(1)).show();
    }

    @Test
    public void maybeShow_previousHostInactive_destroysPreviousAndShowsNew() {
        var coordinator1 = mock(EnterpriseSignalsDisclaimerCoordinator.class);
        var coordinator2 = mock(EnterpriseSignalsDisclaimerCoordinator.class);
        whenCoordinatorCreated().thenReturn(coordinator1).thenReturn(coordinator2);
        var host1 = mock(EnterpriseSignalsDisclaimerHost.class);
        var host2 = mock(EnterpriseSignalsDisclaimerHost.class);
        whenHostCreated().thenReturn(host1).thenReturn(host2);

        StartupEnterpriseSignalsDisclaimerController controller = createManagedControllerAndShow();

        // The host stops being active without reporting a dismissal, e.g. because the queued
        // content was dropped.
        when(host1.isActive()).thenReturn(false);

        Assert.assertTrue(controller.maybeShow(ShownOn.STARTUP));
        verify(host1).destroy();
        verify(coordinator1).destroy();
        verify(host2, never()).destroy();
        verify(coordinator2, never()).destroy();
        verify(mHostFactory).create(anyInt(), eq(coordinator2), any());
        verify(host2).show();
    }

    @Test
    public void destroy_withActiveDisclaimer_destroysCoordinatorAndHost() {
        StartupEnterpriseSignalsDisclaimerController controller = createManagedControllerAndShow();

        controller.destroy();

        verify(mCoordinator).destroy();
        verify(mHost).destroy();
    }

    @Test
    public void controllerCreation_addsSignInStateObserver() {
        StartupEnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        verify(mSigninManager).addSignInStateObserver(controller);
    }

    @Test
    public void destroy_removesSignInStateObserver() {
        StartupEnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        controller.destroy();

        verify(mSigninManager).removeSignInStateObserver(controller);
    }

    @Test
    public void onSignedIn_accountIsManaged_showsDisclaimer() {
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        StartupEnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        controller.onSignedIn();

        verify(mCoordinatorFactory)
                .create(eq(mActivity), eq(mIdentityManager), any(), anyInt(), any());
        verify(mHostFactory).create(anyInt(), eq(mCoordinator), any());
        verify(mHost).show();
    }

    @Test
    public void onSignedIn_accountNotManaged_doesNotShowDisclaimer() {
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(false);

        StartupEnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        controller.onSignedIn();

        verifyCoordinatorCreated(never());
        verifyHostCreated(never());
    }

    @Test
    public void onSignedIn_accountAlreadyAcknowledged_doesNotShowDisclaimer() {
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);
        when(mBridgeNativesMock.hasAccountAcknowledgedSignalsDisclaimer(
                        eq(mProfile), eq(TestAccounts.ACCOUNT1.getGaiaId())))
                .thenReturn(true);

        StartupEnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        controller.onSignedIn();

        verifyCoordinatorCreated(never());
        verifyHostCreated(never());
    }

    @Test
    public void onSignedOut_withActiveDisclaimer_destroysCoordinatorAndHost() {
        StartupEnterpriseSignalsDisclaimerController controller = createManagedControllerAndShow();

        controller.onSignedOut();

        verify(mCoordinator).destroy();
        verify(mHost).destroy();
    }

    @Test
    public void onSignedOut_withoutActiveDisclaimer_doesNotCrash() {
        StartupEnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        controller.onSignedOut();

        verify(mCoordinator, never()).destroy();
        verify(mHost, never()).destroy();
    }

    @Test
    public void onSignedIn_whenControllerDestroyed_doesNotShowDisclaimer() {
        StartupEnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);
        controller.destroy();

        controller.onSignedIn();

        verifyCoordinatorCreated(never());
        verifyHostCreated(never());
    }

    @Test
    public void onSignedIn_disclaimerAlreadyActive_doesNotCreateNewCoordinator() {
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);
        when(mHost.isActive()).thenReturn(true);

        StartupEnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        controller.onSignedIn();
        controller.onSignedIn();

        verifyCoordinatorCreated(times(1));
        verifyHostCreated(times(1));
    }

    @Test
    public void onSignedOut_resetsDisclaimer_allowsShowingAgainOnNextSignIn() {
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        var coordinator1 = mock(EnterpriseSignalsDisclaimerCoordinator.class);
        var coordinator2 = mock(EnterpriseSignalsDisclaimerCoordinator.class);
        whenCoordinatorCreated().thenReturn(coordinator1).thenReturn(coordinator2);
        var host1 = mock(EnterpriseSignalsDisclaimerHost.class);
        var host2 = mock(EnterpriseSignalsDisclaimerHost.class);
        whenHostCreated().thenReturn(host1).thenReturn(host2);

        StartupEnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        controller.onSignedIn();
        verify(host1).show();

        controller.onSignedOut();
        verify(coordinator1).destroy();
        verify(host1).destroy();

        controller.onSignedIn();
        verifyCoordinatorCreated(times(2));
        verify(mHostFactory).create(anyInt(), eq(coordinator2), any());
        verify(host2).show();
    }

    @Test
    public void destroy_afterSignOut_doesNotDoubleDestroy() {
        StartupEnterpriseSignalsDisclaimerController controller = createManagedControllerAndShow();

        controller.onSignedOut();
        verify(mCoordinator, times(1)).destroy();
        verify(mHost, times(1)).destroy();

        controller.destroy();
        verify(mCoordinator, times(1)).destroy();
        verify(mHost, times(1)).destroy();
    }

    @Test
    public void destroy_nullsOutCoordinatorAndHost() {
        StartupEnterpriseSignalsDisclaimerController controller = createManagedControllerAndShow();

        controller.destroy();
        verify(mCoordinator, times(1)).destroy();
        verify(mHost, times(1)).destroy();

        // Subsequent sign out should not destroy them again, because they are null.
        controller.onSignedOut();
        verify(mCoordinator, times(1)).destroy();
        verify(mHost, times(1)).destroy();
    }

    @Test
    public void dismissal_destroysDisclaimer_allowsShowingAgain() {
        StartupEnterpriseSignalsDisclaimerController controller = createManagedControllerAndShow();

        simulateHostDismissed(DismissalCause.DISMISSED_WITHOUT_EXPLICIT_USER_ACTION);
        verify(mCoordinator, times(1)).destroy();
        verify(mHost, times(1)).destroy();

        // The disclaimer has been torn down, so signing out must not destroy it again.
        controller.onSignedOut();
        verify(mCoordinator, times(1)).destroy();
        verify(mHost, times(1)).destroy();

        // The account has not acknowledged the disclaimer, so it can be shown again.
        Assert.assertTrue(controller.maybeShow(ShownOn.STARTUP));
        verifyCoordinatorCreated(times(2));
        verifyHostCreated(times(2));
    }

    @Test
    public void delegate_showInfoPage_invokesCallback() {
        createManagedControllerAndShow();

        captureDelegate().showInfoPage("https://example.com");

        verify(mShowInfoPageCallback).onResult("https://example.com");
    }

    @Test
    public void delegate_onAccept_dismissesHost() {
        createManagedControllerAndShow();

        captureDelegate().onAccept();

        verify(mHost).dismiss(DismissalCause.TAPPED_ACCEPT);
    }

    @Test
    public void delegate_onDecline_dismissesHost() {
        createManagedControllerAndShow();

        captureDelegate().onDecline();

        verify(mHost).dismiss(DismissalCause.TAPPED_SIGN_OUT);
    }

    @Test
    public void delegate_onAcceptAfterSignOut_doesNothing() {
        StartupEnterpriseSignalsDisclaimerController controller = createManagedControllerAndShow();
        EnterpriseSignalsDisclaimerCoordinator.Delegate delegate = captureDelegate();
        controller.onSignedOut();

        delegate.onAccept();

        verify(mHost, never()).dismiss(anyInt());
        verify(mBridgeNativesMock, never()).setAccountAcknowledgedSignalsDisclaimer(any(), any());
    }

    @Test
    public void onSignedIn_accountIsManaged_recordsShowRequestedOnSignIn() {
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        StartupEnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        MetricsHelper.HISTOGRAM_SHOWN_REQUESTED, ShownOn.SIGN_IN);

        controller.onSignedIn();

        histogramWatcher.assertExpected();
    }

    @Test
    public void maybeShowOnStartup_accountIsManaged_recordsShowRequestedOnStartup() {
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        StartupEnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        MetricsHelper.HISTOGRAM_SHOWN_REQUESTED, ShownOn.STARTUP);

        Assert.assertTrue(controller.maybeShowOnStartup());

        histogramWatcher.assertExpected();
    }

    @Test
    public void onDismissed_tappedAccept_acknowledgesDisclaimer() {
        createManagedControllerAndShow();

        simulateHostDismissed(DismissalCause.TAPPED_ACCEPT);

        verify(mBridgeNativesMock)
                .setAccountAcknowledgedSignalsDisclaimer(
                        eq(mProfile), eq(TestAccounts.ACCOUNT1.getGaiaId()));
        verify(mSigninManager, never()).signOut(anyInt());
    }

    @Test
    public void onDismissed_tappedSignOut_signsOutUser() {
        when(mSigninManager.isSignOutAllowed()).thenReturn(true);
        runSignOutOperationsSynchronously();
        createManagedControllerAndShow();

        simulateHostDismissed(DismissalCause.TAPPED_SIGN_OUT);

        verify(mSigninManager)
                .signOut(eq(SignoutReason.USER_DECLINED_ENTERPRISE_SIGNALS_DISCLAIMER));
        verify(mBridgeNativesMock, never()).setAccountAcknowledgedSignalsDisclaimer(any(), any());
    }

    @Test
    public void onDismissed_dismissedBySwipeDown_signsOutUser() {
        when(mSigninManager.isSignOutAllowed()).thenReturn(true);
        runSignOutOperationsSynchronously();
        createManagedControllerAndShow();

        simulateHostDismissed(DismissalCause.DISMISSED_BY_SWIPE_DOWN);

        verify(mSigninManager)
                .signOut(eq(SignoutReason.USER_DECLINED_ENTERPRISE_SIGNALS_DISCLAIMER));
        verify(mBridgeNativesMock, never()).setAccountAcknowledgedSignalsDisclaimer(any(), any());
    }

    @Test
    public void onDismissed_signOutNotAllowed_doesNotSignOut() {
        when(mSigninManager.isSignOutAllowed()).thenReturn(false);
        runSignOutOperationsSynchronously();
        createManagedControllerAndShow();

        simulateHostDismissed(DismissalCause.TAPPED_SIGN_OUT);

        verify(mSigninManager, never()).signOut(anyInt());
        verify(mBridgeNativesMock, never()).setAccountAcknowledgedSignalsDisclaimer(any(), any());
    }

    @Test
    public void onDismissed_withoutExplicitUserAction_neitherAcknowledgesNorSignsOut() {
        createManagedControllerAndShow();

        simulateHostDismissed(DismissalCause.DISMISSED_WITHOUT_EXPLICIT_USER_ACTION);

        verify(mSigninManager, never()).signOut(anyInt());
        verify(mBridgeNativesMock, never()).setAccountAcknowledgedSignalsDisclaimer(any(), any());
    }
}
