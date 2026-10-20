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

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
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
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerController;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerController.ShownOn;
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
    @Mock private EnterpriseSignalsDisclaimerController mDisclaimerController;
    @Captor private ArgumentCaptor<Callback<Boolean>> mIsAccountManagedCallbackCaptor;
    @Captor private ArgumentCaptor<EnterpriseSignalsDisclaimerController.Listener> mListenerCaptor;
    @Captor private ArgumentCaptor<PropertyModel> mDialogModelCaptor;

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
                        mProfileSupplier,
                        mDisclaimerController);
    }

    private void setUpUnconsentedManagedAccount() {
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(TestAccounts.MANAGED_ACCOUNT);
        when(mSigninManager.isSignOutAllowed()).thenReturn(true);
        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mSigninManager)
                .runAfterOperationInProgress(any());
        when(mSigninManager.getUserAcceptedAccountManagement()).thenReturn(false);
        doAnswer(
                        invocation -> {
                            Callback<Boolean> callback = invocation.getArgument(1);
                            callback.onResult(true);
                            return null;
                        })
                .when(mSigninManager)
                .isAccountManaged(eq(TestAccounts.MANAGED_ACCOUNT), any());
    }

    private EnterpriseSignalsDisclaimerController.Listener captureDisclaimerListener() {
        verify(mDisclaimerController)
                .show(
                        eq(mIdentityManager),
                        eq(TestAccounts.MANAGED_ACCOUNT),
                        eq(ShownOn.SIGNIN_STATE_RECOVERY),
                        mListenerCaptor.capture());
        return mListenerCaptor.getValue();
    }

    @Test
    public void testRegistersObserver() {
        verify(mActivityLifecycleDispatcher).register(mController);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.VERIFY_STARTUP_SIGNIN_STATE)
    public void testFeatureDisabled_doesNothing() {
        mController.onFinishNativeInitialization();

        verify(mSigninManager, never()).isAccountManaged(any(), any());
    }

    @Test
    public void testNotSignedIn_doesNothing() {
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(null);

        mController.onFinishNativeInitialization();

        verify(mSigninManager, never()).isAccountManaged(any(), any());
    }

    @Test
    public void testAlreadyConsented_doesNothing() {
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(TestAccounts.MANAGED_ACCOUNT);
        when(mSigninManager.getUserAcceptedAccountManagement()).thenReturn(true);

        mController.onFinishNativeInitialization();

        verify(mSigninManager, never()).isAccountManaged(any(), any());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_NEW_MANAGEMENT_NOTICE)
    public void testUnconsentedManagedAccount_showsDialogAndAccepts() {
        setUpUnconsentedManagedAccount();
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
        verify(mDisclaimerController, never()).show(any(), any(), anyInt(), any());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_NEW_MANAGEMENT_NOTICE)
    public void testUnconsentedManagedAccount_showsDialogAndCancels() {
        setUpUnconsentedManagedAccount();
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
        verify(mSigninManager)
                .signOut(SignoutReason.USER_DECLINED_ENTERPRISE_MANAGEMENT_DISCLAIMER);
        verify(mDisclaimerController, never()).show(any(), any(), anyInt(), any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_NEW_MANAGEMENT_NOTICE)
    public void testUnconsentedManagedAccount_newNotice_showsDisclaimerAndAccepts() {
        setUpUnconsentedManagedAccount();

        mController.onFinishNativeInitialization();

        EnterpriseSignalsDisclaimerController.Listener listener = captureDisclaimerListener();
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
        verify(mSigninManager, never()).setUserAcceptedAccountManagement(anyBoolean());

        listener.onAccepted();

        verify(mSigninManager).setUserAcceptedAccountManagement(true);
        verify(mSigninManager, never()).signOut(anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_NEW_MANAGEMENT_NOTICE)
    public void testUnconsentedManagedAccount_newNotice_showsDisclaimerAndDeclines() {
        setUpUnconsentedManagedAccount();

        mController.onFinishNativeInitialization();

        EnterpriseSignalsDisclaimerController.Listener listener = captureDisclaimerListener();
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());

        listener.onDeclined();

        verify(mSigninManager).setUserAcceptedAccountManagement(false);
        verify(mSigninManager)
                .signOut(SignoutReason.USER_DECLINED_ENTERPRISE_MANAGEMENT_DISCLAIMER);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_NEW_MANAGEMENT_NOTICE)
    public void testUnconsentedManagedAccount_newNotice_signedOutBeforeAccepting_noEffect() {
        setUpUnconsentedManagedAccount();
        mController.onFinishNativeInitialization();
        EnterpriseSignalsDisclaimerController.Listener listener = captureDisclaimerListener();

        simulateSignOut();
        listener.onAccepted();

        verifyDisclaimerHadNoEffect();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_NEW_MANAGEMENT_NOTICE)
    public void testUnconsentedManagedAccount_newNotice_signedOutBeforeDeclining_noEffect() {
        setUpUnconsentedManagedAccount();
        mController.onFinishNativeInitialization();
        EnterpriseSignalsDisclaimerController.Listener listener = captureDisclaimerListener();

        simulateSignOut();
        listener.onDeclined();

        verifyDisclaimerHadNoEffect();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_NEW_MANAGEMENT_NOTICE)
    public void testUnconsentedManagedAccount_signedOutBeforeAccepting_noEffect() {
        setUpUnconsentedManagedAccount();
        mController.onFinishNativeInitialization();
        PropertyModel dialogModel = captureManagementNoticeDialogModel();

        simulateSignOut();
        dialogModel
                .get(ModalDialogProperties.CONTROLLER)
                .onClick(dialogModel, ModalDialogProperties.ButtonType.POSITIVE);

        verifyDisclaimerHadNoEffect();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_NEW_MANAGEMENT_NOTICE)
    public void testUnconsentedManagedAccount_signedOutBeforeCancelling_noEffect() {
        setUpUnconsentedManagedAccount();
        mController.onFinishNativeInitialization();
        PropertyModel dialogModel = captureManagementNoticeDialogModel();

        simulateSignOut();
        dialogModel
                .get(ModalDialogProperties.CONTROLLER)
                .onDismiss(dialogModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);

        verifyDisclaimerHadNoEffect();
    }

    /** Returns the model of the legacy management notice dialog shown by the controller. */
    private PropertyModel captureManagementNoticeDialogModel() {
        verify(mModalDialogManager).showDialog(mDialogModelCaptor.capture(), anyInt());
        verify(mDisclaimerController, never()).show(any(), any(), anyInt(), any());
        return mDialogModelCaptor.getValue();
    }

    /**
     * Simulates the user being signed out (e.g. from another window or by removing the account from
     * the device).
     */
    private void simulateSignOut() {
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(null);
        when(mSigninManager.isSignOutAllowed()).thenReturn(false);
    }

    /** Verifies that the disclaimer interaction didn't change the sign-in or consent state. */
    private void verifyDisclaimerHadNoEffect() {
        verify(mSigninManager, never()).setUserAcceptedAccountManagement(anyBoolean());
        verify(mSigninManager, never()).signOut(anyInt());
        verify(mSigninManager, never()).signOut(anyInt(), any());
    }

    @Test
    public void testOnDestroy_destroysDisclaimerController() {
        mController.onDestroy();

        verify(mDisclaimerController).destroy();
        verify(mActivityLifecycleDispatcher).unregister(mController);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_NEW_MANAGEMENT_NOTICE)
    public void testOnDestroy_beforeManagedStatusFetched_doesNotShowDisclaimer() {
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(TestAccounts.MANAGED_ACCOUNT);
        when(mSigninManager.getUserAcceptedAccountManagement()).thenReturn(false);

        mController.onFinishNativeInitialization();
        verify(mSigninManager)
                .isAccountManaged(
                        eq(TestAccounts.MANAGED_ACCOUNT),
                        mIsAccountManagedCallbackCaptor.capture());
        mController.onDestroy();
        mIsAccountManagedCallbackCaptor.getValue().onResult(true);

        verify(mDisclaimerController, never()).show(any(), any(), anyInt(), any());
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
    }

    @Test
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
