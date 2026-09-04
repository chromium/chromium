// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtilsJni;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerController.CoordinatorFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.test.util.FakeIdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit tests for {@link EnterpriseSignalsDisclaimerController}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
public class EnterpriseSignalsDisclaimerControllerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private SigninManager mSigninManager;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private AppCompatActivity mActivity;
    @Mock private EnterpriseSignalsDisclaimerCoordinator mCoordinator;
    @Mock private CoordinatorFactory mCoordinatorFactory;
    @Mock private EnterpriseSignalsDisclaimerCoordinator.Delegate mDelegate;
    @Mock private ManagedBrowserUtils.Natives mManagedBrowserUtilsJniMock;
    @Mock private EnterpriseSignalsDisclaimerBridge.Natives mBridgeNativesMock;

    private final FakeIdentityManager mIdentityManager = new FakeIdentityManager();

    @Before
    public void setUp() {
        ManagedBrowserUtilsJni.setInstanceForTesting(mManagedBrowserUtilsJniMock);
        EnterpriseSignalsDisclaimerBridgeJni.setInstanceForTesting(mBridgeNativesMock);
        IdentityServicesProvider.setSigninManagerForTesting(mSigninManager);

        when(mSigninManager.getIdentityManager()).thenReturn(mIdentityManager);
        when(mCoordinatorFactory.create(any(), any(), any(), any(), any(), any()))
                .thenReturn(mCoordinator);
        when(mBridgeNativesMock.hasAccountAcknowledgedSignalsDisclaimer(any())).thenReturn(false);

        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);
    }

    @After
    public void tearDown() {
        ManagedBrowserUtilsJni.setInstanceForTesting(null);
        EnterpriseSignalsDisclaimerBridgeJni.setInstanceForTesting(null);
    }

    private EnterpriseSignalsDisclaimerController createController() {
        return EnterpriseSignalsDisclaimerController.maybeCreateForProfile(
                mProfile,
                mBottomSheetController,
                mModalDialogManager,
                mActivity,
                mDelegate,
                mCoordinatorFactory);
    }

    @Test
    public void maybeCreateForProfile_offTheRecordProfile_returnsNull() {
        when(mProfile.isOffTheRecord()).thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();

        Assert.assertNull(controller);
    }

    @Test
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void maybeCreateForProfile_hasDisableFirstRunExperienceSwitch_returnsNull() {
        when(mProfile.isOffTheRecord()).thenReturn(false);

        EnterpriseSignalsDisclaimerController controller = createController();

        Assert.assertNull(controller);
    }

    @Test
    public void maybeCreateForProfile_validProfile_returnsInstance() {
        when(mProfile.isOffTheRecord()).thenReturn(false);

        EnterpriseSignalsDisclaimerController controller = createController();

        Assert.assertNotNull(controller);
    }

    @Test
    public void maybeCreateForProfile_validProfileNoPrimaryAccount_returnsInstance() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mIdentityManager.setPrimaryAccount(null);

        EnterpriseSignalsDisclaimerController controller = createController();

        Assert.assertNotNull(controller);
    }

    @Test
    public void maybeShow_destroyed_returnsFalse() {
        when(mProfile.isOffTheRecord()).thenReturn(false);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        controller.destroy();

        Assert.assertFalse(controller.maybeShow());
        verify(mCoordinatorFactory, never()).create(any(), any(), any(), any(), any(), any());
    }

    @Test
    public void maybeShow_noPrimaryAccount_returnsFalse() {
        when(mProfile.isOffTheRecord()).thenReturn(false);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        mIdentityManager.setPrimaryAccount(null);

        Assert.assertFalse(controller.maybeShow());
        verify(mCoordinatorFactory, never()).create(any(), any(), any(), any(), any(), any());
        verify(mCoordinator, never()).show();
    }

    @Test
    public void maybeShow_profileNotManaged_returnsFalse() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(false);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        Assert.assertFalse(controller.maybeShow());
        verify(mCoordinatorFactory, never()).create(any(), any(), any(), any(), any(), any());
        verify(mCoordinator, never()).show();
    }

    @Test
    public void maybeShow_accountIsManaged_showsDisclaimerAndReturnsTrue() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();

        Assert.assertNotNull(controller);
        Assert.assertTrue(controller.maybeShow());
        verify(mBridgeNativesMock)
                .hasAccountAcknowledgedSignalsDisclaimer(eq(TestAccounts.ACCOUNT1.getGaiaId()));
        verify(mCoordinatorFactory)
                .create(
                        eq(mActivity),
                        eq(mBottomSheetController),
                        eq(mModalDialogManager),
                        eq(mSigninManager),
                        eq(mDelegate),
                        any());
        verify(mCoordinator).show();
    }

    @Test
    public void maybeShow_accountAlreadyAcknowledged_returnsFalse() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);
        when(mBridgeNativesMock.hasAccountAcknowledgedSignalsDisclaimer(
                        eq(TestAccounts.ACCOUNT1.getGaiaId())))
                .thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        Assert.assertFalse(controller.maybeShow());
        verify(mCoordinatorFactory, never()).create(any(), any(), any(), any(), any(), any());
        verify(mCoordinator, never()).show();
    }

    @Test
    public void maybeShow_alreadyShowing_returnsFalse() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);
        Assert.assertTrue(controller.maybeShow());

        when(mCoordinator.isActive()).thenReturn(true);
        Assert.assertFalse(controller.maybeShow());

        verify(mCoordinator).isActive();
    }

    @Test
    public void maybeShow_emptyGaiaId_returnsFalse() {
        mIdentityManager.setPrimaryAccount(null);

        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);
        mIdentityManager.setPrimaryAccount(
                new AccountInfo.Builder(TestAccounts.MANAGED_ACCOUNT.getEmail(), new GaiaId(""))
                        .fullName(TestAccounts.MANAGED_ACCOUNT.getFullName())
                        .givenName(TestAccounts.MANAGED_ACCOUNT.getGivenName())
                        .accountImage(TestAccounts.MANAGED_ACCOUNT.getAccountImage())
                        .accountCapabilities(TestAccounts.MANAGED_ACCOUNT.getAccountCapabilities())
                        .build());

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        Assert.assertFalse(controller.maybeShow());
        verify(mCoordinatorFactory, never()).create(any(), any(), any(), any(), any(), any());
        verify(mCoordinator, never()).show();
    }

    @Test
    public void destroy_withActiveCoordinator_destroysCoordinator() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);
        Assert.assertTrue(controller.maybeShow());

        controller.destroy();

        verify(mCoordinator).destroy();
    }

    @Test
    public void maybeShow_destroysPreviousCoordinator() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        EnterpriseSignalsDisclaimerCoordinator coordinator1 =
                mock(EnterpriseSignalsDisclaimerCoordinator.class);
        EnterpriseSignalsDisclaimerCoordinator coordinator2 =
                mock(EnterpriseSignalsDisclaimerCoordinator.class);
        when(mCoordinatorFactory.create(any(), any(), any(), any(), any(), any()))
                .thenReturn(coordinator1)
                .thenReturn(coordinator2);
        when(coordinator1.isActive()).thenReturn(false);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        // First call creates coordinator1.
        Assert.assertTrue(controller.maybeShow());
        verify(mCoordinatorFactory)
                .create(
                        eq(mActivity),
                        eq(mBottomSheetController),
                        eq(mModalDialogManager),
                        eq(mSigninManager),
                        eq(mDelegate),
                        any());

        // Second call should destroy coordinator1 and create coordinator2.
        Assert.assertTrue(controller.maybeShow());
        verify(coordinator1).destroy();
        verify(mCoordinatorFactory, times(2))
                .create(
                        eq(mActivity),
                        eq(mBottomSheetController),
                        eq(mModalDialogManager),
                        eq(mSigninManager),
                        eq(mDelegate),
                        any());
        verify(coordinator2).show();
    }

    @Test
    public void controllerCreation_addsSignInStateObserver() {
        when(mProfile.isOffTheRecord()).thenReturn(false);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        verify(mSigninManager).addSignInStateObserver(controller);
    }

    @Test
    public void destroy_removesSignInStateObserver() {
        when(mProfile.isOffTheRecord()).thenReturn(false);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        controller.destroy();

        verify(mSigninManager).removeSignInStateObserver(controller);
    }

    @Test
    public void onSignedIn_accountIsManaged_showsDisclaimer() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        controller.onSignedIn();

        verify(mCoordinatorFactory)
                .create(
                        eq(mActivity),
                        eq(mBottomSheetController),
                        eq(mModalDialogManager),
                        eq(mSigninManager),
                        eq(mDelegate),
                        any());
        verify(mCoordinator).show();
    }

    @Test
    public void onSignedIn_accountNotManaged_doesNotShowDisclaimer() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(false);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        controller.onSignedIn();

        verify(mCoordinatorFactory, never()).create(any(), any(), any(), any(), any(), any());
        verify(mCoordinator, never()).show();
    }

    @Test
    public void onSignedIn_accountAlreadyAcknowledged_doesNotShowDisclaimer() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);
        when(mBridgeNativesMock.hasAccountAcknowledgedSignalsDisclaimer(
                        eq(TestAccounts.ACCOUNT1.getGaiaId())))
                .thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        controller.onSignedIn();

        verify(mCoordinatorFactory, never()).create(any(), any(), any(), any(), any(), any());
        verify(mCoordinator, never()).show();
    }

    @Test
    public void onSignedOut_withActiveCoordinator_destroysCoordinator() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);
        Assert.assertTrue(controller.maybeShow());

        controller.onSignedOut();

        verify(mCoordinator).destroy();
    }

    @Test
    public void onSignedOut_withoutActiveCoordinator_doesNotCrash() {
        when(mProfile.isOffTheRecord()).thenReturn(false);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        controller.onSignedOut();

        verify(mCoordinator, never()).destroy();
    }

    @Test
    public void onSignedIn_whenControllerDestroyed_doesNotShowDisclaimer() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);
        controller.destroy();

        controller.onSignedIn();

        verify(mCoordinatorFactory, never()).create(any(), any(), any(), any(), any(), any());
        verify(mCoordinator, never()).show();
    }

    @Test
    public void onSignedIn_coordinatorAlreadyActive_doesNotCreateNewCoordinator() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        controller.onSignedIn();
        when(mCoordinator.isActive()).thenReturn(true);

        controller.onSignedIn();

        verify(mCoordinatorFactory, times(1)).create(any(), any(), any(), any(), any(), any());
        verify(mCoordinator, times(1)).show();
    }

    @Test
    public void onSignedOut_resetsCoordinator_allowsShowingAgainOnNextSignIn() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        EnterpriseSignalsDisclaimerCoordinator coordinator1 =
                mock(EnterpriseSignalsDisclaimerCoordinator.class);
        EnterpriseSignalsDisclaimerCoordinator coordinator2 =
                mock(EnterpriseSignalsDisclaimerCoordinator.class);
        when(mCoordinatorFactory.create(any(), any(), any(), any(), any(), any()))
                .thenReturn(coordinator1)
                .thenReturn(coordinator2);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);
        when(coordinator1.isActive()).thenReturn(true);

        controller.onSignedIn();
        verify(coordinator1).show();

        controller.onSignedOut();
        verify(coordinator1).destroy();

        controller.onSignedIn();
        verify(coordinator2).show();
    }

    @Test
    public void destroy_afterSignOut_doesNotDoubleDestroyCoordinator() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);
        Assert.assertTrue(controller.maybeShow());

        controller.onSignedOut();
        verify(mCoordinator, times(1)).destroy();

        controller.destroy();
        verify(mCoordinator, times(1)).destroy();
    }

    @Test
    public void destroy_nullsOutCoordinator() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);
        Assert.assertTrue(controller.maybeShow());

        controller.destroy();
        verify(mCoordinator, times(1)).destroy();

        // Subsequent sign out should not destroy the coordinator again, because it is null.
        controller.onSignedOut();
        verify(mCoordinator, times(1)).destroy();
    }

    @Test
    public void coordinatorDestroyedCallback_nullsOutCoordinator() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);
        Assert.assertTrue(controller.maybeShow());

        var callbackCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mCoordinatorFactory)
                .create(
                        eq(mActivity),
                        eq(mBottomSheetController),
                        eq(mModalDialogManager),
                        eq(mSigninManager),
                        eq(mDelegate),
                        callbackCaptor.capture());

        // Simulate the coordinator destroying itself.
        callbackCaptor.getValue().run();

        // Now we verify that destroy is not called on the coordinator again, because it should have
        // been nulled out.
        controller.onSignedOut();
        verify(mCoordinator, never()).destroy();
    }
}
