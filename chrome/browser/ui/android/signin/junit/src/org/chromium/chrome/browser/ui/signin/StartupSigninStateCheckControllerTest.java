// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link StartupSigninStateCheckController}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.VERIFY_STARTUP_SIGNIN_STATE)
public class StartupSigninStateCheckControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    private final SettableMonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private SigninManager mSigninManager;
    @Mock private IdentityManager mIdentityManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    private final Context mContext = ApplicationProvider.getApplicationContext();

    private StartupSigninStateCheckController mController;

    @Before
    public void setUp() {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        mProfileSupplier.set(mProfile);
        when(mIdentityServicesProvider.getSigninManager(mProfile)).thenReturn(mSigninManager);
        when(mSigninManager.getIdentityManager()).thenReturn(mIdentityManager);
        when(mSigninManager.extractDomainName(anyString())).thenReturn("managed.com");

        mController =
                new StartupSigninStateCheckController(
                        mContext,
                        mModalDialogManager,
                        mActivityLifecycleDispatcher,
                        mProfileSupplier);
    }

    @Test
    @SmallTest
    public void testRegistersObserver() {
        verify(mActivityLifecycleDispatcher).register(mController);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.VERIFY_STARTUP_SIGNIN_STATE)
    public void testFeatureDisabled_doesNothing() {
        mController.onFinishNativeInitialization();

        verify(mSigninManager, never()).isAccountManaged(any(), any());
    }

    @Test
    @SmallTest
    public void testNotSignedIn_doesNothing() {
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(null);

        mController.onFinishNativeInitialization();

        verify(mSigninManager, never()).isAccountManaged(any(), any());
    }

    @Test
    @SmallTest
    public void testAlreadyConsented_doesNothing() {
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(TestAccounts.MANAGED_ACCOUNT);
        when(mSigninManager.getUserAcceptedAccountManagement()).thenReturn(true);

        mController.onFinishNativeInitialization();

        verify(mSigninManager, never()).isAccountManaged(any(), any());
    }

    @Test
    @SmallTest
    public void testUnconsentedManagedAccount_showsDialogAndAccepts() {
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(TestAccounts.MANAGED_ACCOUNT);
        when(mSigninManager.getUserAcceptedAccountManagement()).thenReturn(false);
        doAnswer(
                        invocation -> {
                            Callback<Boolean> callback = invocation.getArgument(1);
                            callback.onResult(true);
                            return null;
                        })
                .when(mSigninManager)
                .isAccountManaged(eq(TestAccounts.MANAGED_ACCOUNT), any());
        doAnswer(
                        invocation -> {
                            PropertyModel propertyModel = invocation.getArgument(0);
                            ModalDialogProperties.Controller controller =
                                    propertyModel.get(ModalDialogProperties.CONTROLLER);
                            controller.onClick(
                                    propertyModel, ModalDialogProperties.ButtonType.POSITIVE);
                            return null;
                        })
                .when(mModalDialogManager)
                .showDialog(any(), anyInt());

        mController.onFinishNativeInitialization();

        verify(mSigninManager).isAccountManaged(eq(TestAccounts.MANAGED_ACCOUNT), any());
        verify(mSigninManager).setUserAcceptedAccountManagement(true);
    }

    @Test
    @SmallTest
    public void testUnconsentedManagedAccount_showsDialogAndCancels() {
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(TestAccounts.MANAGED_ACCOUNT);
        when(mSigninManager.getUserAcceptedAccountManagement()).thenReturn(false);
        doAnswer(
                        invocation -> {
                            Callback<Boolean> callback = invocation.getArgument(1);
                            callback.onResult(true);
                            return null;
                        })
                .when(mSigninManager)
                .isAccountManaged(eq(TestAccounts.MANAGED_ACCOUNT), any());
        doAnswer(
                        invocation -> {
                            PropertyModel propertyModel = invocation.getArgument(0);
                            ModalDialogProperties.Controller controller =
                                    propertyModel.get(ModalDialogProperties.CONTROLLER);
                            controller.onDismiss(
                                    propertyModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                            return null;
                        })
                .when(mModalDialogManager)
                .showDialog(any(), anyInt());

        mController.onFinishNativeInitialization();

        verify(mSigninManager).isAccountManaged(eq(TestAccounts.MANAGED_ACCOUNT), any());
        verify(mSigninManager).setUserAcceptedAccountManagement(false);
        verify(mSigninManager).signOut(SignoutReason.ABORT_SIGNIN);
    }

    @Test
    @SmallTest
    public void testUnmanagedAccount_doesNothing() {
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(TestAccounts.ACCOUNT1);
        when(mSigninManager.getUserAcceptedAccountManagement()).thenReturn(false);
        doAnswer(
                        invocation -> {
                            Callback<Boolean> callback = invocation.getArgument(1);
                            callback.onResult(false);
                            return null;
                        })
                .when(mSigninManager)
                .isAccountManaged(eq(TestAccounts.ACCOUNT1), any());

        mController.onFinishNativeInitialization();

        verify(mSigninManager).isAccountManaged(eq(TestAccounts.ACCOUNT1), any());
        verify(mSigninManager, never()).setUserAcceptedAccountManagement(anyBoolean());
    }
}
