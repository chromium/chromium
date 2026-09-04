// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.Spanned;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.GraphicsMode;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
public class EnterpriseSignalsDisclaimerMediatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private EnterpriseSignalsDisclaimerBridge.Natives mBridgeNativesMock;
    @Mock private EnterpriseSignalsDisclaimerCoordinator.Delegate mDelegate;
    @Mock private SigninManager mSigninManager;
    @Mock private Runnable mHideDialogCallback;

    @Before
    public void setUp() {
        EnterpriseSignalsDisclaimerBridgeJni.setInstanceForTesting(mBridgeNativesMock);

        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mSigninManager)
                .runAfterOperationInProgress(any());
    }

    @After
    public void tearDown() {
        EnterpriseSignalsDisclaimerBridgeJni.setInstanceForTesting(null);
    }

    private EnterpriseSignalsDisclaimerMediator createMediatorForAccount(AccountInfo accountInfo) {
        mAccountManagerTestRule.addAccount(accountInfo);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(accountInfo);
        return new EnterpriseSignalsDisclaimerMediator(
                ContextUtils.getApplicationContext(),
                mAccountManagerTestRule.getIdentityManager(),
                mDelegate,
                mSigninManager,
                mHideDialogCallback);
    }

    @Test
    public void primaryAccount_profilePicture() {
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();
        Assert.assertNotNull(model.get(EnterpriseSignalsDisclaimerProperties.PROFILE_PICTURE));
    }

    @Test
    @GraphicsMode(GraphicsMode.Mode.NATIVE)
    public void profileDataUpdated_profilePicture() {
        AccountInfo accountWithoutImage =
                new AccountInfo.Builder(TestAccounts.MANAGED_ACCOUNT).accountImage(null).build();
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(accountWithoutImage);

        PropertyModel model = mediator.getModel();
        Drawable initialPicture = model.get(EnterpriseSignalsDisclaimerProperties.PROFILE_PICTURE);
        Assert.assertNotNull(initialPicture);
        Bitmap initialBitmap = ((BitmapDrawable) initialPicture).getBitmap();

        mAccountManagerTestRule.updateAccount(TestAccounts.MANAGED_ACCOUNT);

        Drawable updatedPicture = model.get(EnterpriseSignalsDisclaimerProperties.PROFILE_PICTURE);
        Assert.assertNotNull(updatedPicture);
        Bitmap updatedBitmap = ((BitmapDrawable) updatedPicture).getBitmap();

        Assert.assertFalse(
                "The updated profile picture should have a different bitmap from the initial one.",
                initialBitmap.sameAs(updatedBitmap));
    }

    @Test
    public void learnMoreLink_callsCustomTabCallback() {
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();
        CharSequence description = model.get(EnterpriseSignalsDisclaimerProperties.DESCRIPTION);
        Spanned spanned = (Spanned) description;
        ChromeClickableSpan[] spans =
                spanned.getSpans(0, spanned.length(), ChromeClickableSpan.class);
        Assert.assertEquals(1, spans.length);
        spans[0].onClick(null);
        verify(mDelegate).showInfoPage(eq(EnterpriseSignalsDisclaimerMediator.LEARN_MORE_LINK));
    }

    @Test
    public void onAcceptButtonClicked_runsHideDialogCallback() {
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();

        model.get(EnterpriseSignalsDisclaimerProperties.ON_ACCEPT_CLICKED).onClick(null);

        verify(mHideDialogCallback).run();
        verify(mBridgeNativesMock)
                .setAccountAcknowledgedSignalsDisclaimer(
                        eq(TestAccounts.MANAGED_ACCOUNT.getGaiaId()));
    }

    @Test
    public void onCancelButtonClicked_hidesDialogAndSignsOutUser() {
        when(mSigninManager.isSignOutAllowed()).thenReturn(true);
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();

        model.get(EnterpriseSignalsDisclaimerProperties.ON_CANCEL_CLICKED).onClick(null);

        verify(mHideDialogCallback).run();
        verify(mSigninManager)
                .signOut(eq(SignoutReason.USER_DECLINED_ENTERPRISE_SIGNALS_DISCLAIMER));
        verify(mBridgeNativesMock, never()).setAccountAcknowledgedSignalsDisclaimer(any());
    }

    @Test
    public void signOutUser_signOutNotAllowed_doesNotSignOut() {
        when(mSigninManager.isSignOutAllowed()).thenReturn(false);
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);

        mediator.signOutUser();

        verify(mSigninManager, never()).signOut(anyInt());
        verify(mBridgeNativesMock, never()).setAccountAcknowledgedSignalsDisclaimer(any());
    }

    @Test
    public void onAcceptButtonClicked_calledTwice_hidesDialogOnlyOnce() {
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();

        var onAccept = model.get(EnterpriseSignalsDisclaimerProperties.ON_ACCEPT_CLICKED);
        onAccept.onClick(null);
        onAccept.onClick(null);

        verify(mHideDialogCallback, times(1)).run();
        verify(mBridgeNativesMock, times(1))
                .setAccountAcknowledgedSignalsDisclaimer(
                        eq(TestAccounts.MANAGED_ACCOUNT.getGaiaId()));
    }

    @Test
    public void onCancelButtonClicked_calledTwice_signsOutAndHidesDialogOnlyOnce() {
        when(mSigninManager.isSignOutAllowed()).thenReturn(true);
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();

        var onCancel = model.get(EnterpriseSignalsDisclaimerProperties.ON_CANCEL_CLICKED);
        onCancel.onClick(null);
        onCancel.onClick(null);

        verify(mHideDialogCallback, times(1)).run();
        verify(mSigninManager, times(1))
                .signOut(eq(SignoutReason.USER_DECLINED_ENTERPRISE_SIGNALS_DISCLAIMER));
        verify(mBridgeNativesMock, never()).setAccountAcknowledgedSignalsDisclaimer(any());
    }

    @Test
    public void onAcceptButtonClicked_thenCancelButtonClicked_ignoresSecondClick() {
        when(mSigninManager.isSignOutAllowed()).thenReturn(true);
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();

        model.get(EnterpriseSignalsDisclaimerProperties.ON_ACCEPT_CLICKED).onClick(null);
        model.get(EnterpriseSignalsDisclaimerProperties.ON_CANCEL_CLICKED).onClick(null);

        verify(mHideDialogCallback, times(1)).run();
        verify(mSigninManager, never()).signOut(anyInt());
        verify(mBridgeNativesMock, times(1))
                .setAccountAcknowledgedSignalsDisclaimer(
                        eq(TestAccounts.MANAGED_ACCOUNT.getGaiaId()));
    }

    @Test
    public void onCancelButtonClicked_thenAcceptButtonClicked_ignoresSecondClick() {
        when(mSigninManager.isSignOutAllowed()).thenReturn(true);
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();

        model.get(EnterpriseSignalsDisclaimerProperties.ON_CANCEL_CLICKED).onClick(null);
        model.get(EnterpriseSignalsDisclaimerProperties.ON_ACCEPT_CLICKED).onClick(null);

        verify(mHideDialogCallback, times(1)).run();
        verify(mSigninManager, times(1))
                .signOut(eq(SignoutReason.USER_DECLINED_ENTERPRISE_SIGNALS_DISCLAIMER));
        verify(mBridgeNativesMock, never()).setAccountAcknowledgedSignalsDisclaimer(any());
    }

    @Test
    public void onAcceptButtonClicked_ThenSignoutTriggeredByDismissal() {
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();

        model.get(EnterpriseSignalsDisclaimerProperties.ON_ACCEPT_CLICKED).onClick(null);
        mediator.signOutUser();

        verify(mHideDialogCallback, times(1)).run();
        verify(mSigninManager, times(0)).signOut(anyInt());
        verify(mBridgeNativesMock, times(1))
                .setAccountAcknowledgedSignalsDisclaimer(
                        eq(TestAccounts.MANAGED_ACCOUNT.getGaiaId()));
    }

    @Test
    public void onCancelButtonClicked_ThenSignoutTriggeredByDismissal() {
        when(mSigninManager.isSignOutAllowed()).thenReturn(true);
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();

        model.get(EnterpriseSignalsDisclaimerProperties.ON_CANCEL_CLICKED).onClick(null);
        mediator.signOutUser();

        verify(mHideDialogCallback, times(1)).run();
        verify(mSigninManager, times(1)).signOut(anyInt());
        verify(mBridgeNativesMock, never()).setAccountAcknowledgedSignalsDisclaimer(any());
    }

    @Test
    public void onDialogDismissed_clickingAcceptIgnored() {
        when(mSigninManager.isSignOutAllowed()).thenReturn(true);
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();

        mediator.signOutUser();
        model.get(EnterpriseSignalsDisclaimerProperties.ON_ACCEPT_CLICKED).onClick(null);

        verify(mHideDialogCallback, times(0)).run();
        verify(mSigninManager, times(1)).signOut(anyInt());
        verify(mBridgeNativesMock, never()).setAccountAcknowledgedSignalsDisclaimer(any());
    }

    @Test
    public void onDialogDismissed_clickingCancelDoesNotSignoutAgain() {
        when(mSigninManager.isSignOutAllowed()).thenReturn(true);
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();

        mediator.signOutUser();
        model.get(EnterpriseSignalsDisclaimerProperties.ON_CANCEL_CLICKED).onClick(null);

        verify(mHideDialogCallback, times(0)).run();
        verify(mSigninManager, times(1)).signOut(anyInt());
        verify(mBridgeNativesMock, never()).setAccountAcknowledgedSignalsDisclaimer(any());
    }
}
