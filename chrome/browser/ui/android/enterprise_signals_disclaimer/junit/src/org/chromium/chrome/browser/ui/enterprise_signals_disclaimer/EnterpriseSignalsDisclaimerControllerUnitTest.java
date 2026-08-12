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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.CommandLine;
import org.chromium.base.test.BaseRobolectricTestRunner;
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
import org.chromium.components.signin.test.util.FakeIdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;

/** Unit tests for {@link EnterpriseSignalsDisclaimerController}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
public class EnterpriseSignalsDisclaimerControllerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private SigninManager mSigninManager;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private AppCompatActivity mActivity;
    @Mock private EnterpriseSignalsDisclaimerCoordinator mCoordinator;
    @Mock private CoordinatorFactory mCoordinatorFactory;
    @Mock private EnterpriseSignalsDisclaimerCoordinator.Delegate mDelegate;
    @Mock private ManagedBrowserUtils.Natives mManagedBrowserUtilsJniMock;

    private final FakeIdentityManager mIdentityManager = new FakeIdentityManager();

    @Before
    public void setUp() {
        ManagedBrowserUtilsJni.setInstanceForTesting(mManagedBrowserUtilsJniMock);
        IdentityServicesProvider.setSigninManagerForTesting(mSigninManager);

        when(mSigninManager.getIdentityManager()).thenReturn(mIdentityManager);
        when(mCoordinatorFactory.create(any(), any(), any(), any())).thenReturn(mCoordinator);
        when(mCoordinator.show()).thenReturn(true);

        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);
    }

    @After
    public void tearDown() {
        CommandLine.getInstance().removeSwitch(ChromeSwitches.NO_FIRST_RUN);
    }

    private EnterpriseSignalsDisclaimerController createController() {
        return EnterpriseSignalsDisclaimerController.maybeCreateForProfile(
                mProfile, mBottomSheetController, mActivity, mDelegate, mCoordinatorFactory);
    }

    @Test
    public void maybeCreateForProfile_offTheRecordProfile_returnsNull() {
        when(mProfile.isOffTheRecord()).thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();

        Assert.assertNull(controller);
    }

    @Test
    public void maybeCreateForProfile_hasNoFirstRunSwitch_returnsNull() {
        CommandLine.getInstance().appendSwitch(ChromeSwitches.NO_FIRST_RUN);
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
        verify(mCoordinatorFactory, never()).create(any(), any(), any(), any());
    }

    @Test
    public void maybeShow_noPrimaryAccount_returnsFalse() {
        when(mProfile.isOffTheRecord()).thenReturn(false);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        mIdentityManager.setPrimaryAccount(null);

        Assert.assertFalse(controller.maybeShow());
        verify(mCoordinatorFactory, never()).create(any(), any(), any(), any());
        verify(mCoordinator, never()).show();
    }

    @Test
    public void maybeShow_profileNotManaged_returnsFalse() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(false);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        Assert.assertFalse(controller.maybeShow());
        verify(mCoordinatorFactory, never()).create(any(), any(), any(), any());
        verify(mCoordinator, never()).show();
    }

    @Test
    public void maybeShow_accountIsManaged_showsDisclaimerAndReturnsTrue() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();

        Assert.assertNotNull(controller);
        Assert.assertTrue(controller.maybeShow());
        verify(mCoordinatorFactory)
                .create(
                        eq(mActivity),
                        eq(mBottomSheetController),
                        eq(mSigninManager),
                        eq(mDelegate));
        verify(mCoordinator).show();
    }

    @Test
    public void maybeShow_alreadyShowing_returnsFalse() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isProfileManaged(mProfile)).thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);
        Assert.assertTrue(controller.maybeShow());

        when(mCoordinator.isShowing()).thenReturn(true);
        Assert.assertFalse(controller.maybeShow());

        verify(mCoordinator).isShowing();
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
        when(mCoordinatorFactory.create(any(), any(), any(), any()))
                .thenReturn(coordinator1)
                .thenReturn(coordinator2);
        when(coordinator1.show()).thenReturn(true);
        when(coordinator1.isShowing()).thenReturn(false);
        when(coordinator2.show()).thenReturn(true);

        EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        // First call creates coordinator1.
        Assert.assertTrue(controller.maybeShow());
        verify(mCoordinatorFactory)
                .create(
                        eq(mActivity),
                        eq(mBottomSheetController),
                        eq(mSigninManager),
                        eq(mDelegate));

        // Second call should destroy coordinator1 and create coordinator2.
        Assert.assertTrue(controller.maybeShow());
        verify(coordinator1).destroy();
        verify(mCoordinatorFactory, times(2))
                .create(
                        eq(mActivity),
                        eq(mBottomSheetController),
                        eq(mSigninManager),
                        eq(mDelegate));
        verify(coordinator2).show();
    }
}
