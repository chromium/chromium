// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
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
    @Mock private EnterpriseSignalsDisclaimerMediator.Delegate mDelegate;

    @Before
    public void setUp() {
        EnterpriseSignalsDisclaimerBridgeJni.setInstanceForTesting(mBridgeNativesMock);
    }

    @After
    public void tearDown() {
        EnterpriseSignalsDisclaimerBridgeJni.setInstanceForTesting(null);
    }

    private EnterpriseSignalsDisclaimerMediator createMediatorForAccount(AccountInfo accountInfo) {
        mAccountManagerTestRule.addAccount(accountInfo);
        return new EnterpriseSignalsDisclaimerMediator(
                ContextUtils.getApplicationContext(),
                mAccountManagerTestRule.getIdentityManager(),
                accountInfo,
                mDelegate);
    }

    @Test
    public void account_profilePicture() {
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
    public void onAcceptButtonClicked_notifiesDelegate() {
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();

        model.get(EnterpriseSignalsDisclaimerProperties.ON_ACCEPT_CLICKED).onClick(null);

        verify(mDelegate).onAccept();
        // Acknowledging the disclaimer is the embedder's responsibility, not the mediator's.
        verify(mBridgeNativesMock, never()).setAccountAcknowledgedSignalsDisclaimer(any());
    }

    @Test
    public void onCancelButtonClicked_notifiesDelegate() {
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();

        model.get(EnterpriseSignalsDisclaimerProperties.ON_CANCEL_CLICKED).onClick(null);

        verify(mDelegate).onDecline();
        verify(mBridgeNativesMock, never()).setAccountAcknowledgedSignalsDisclaimer(any());
    }

    @Test
    public void onAcceptButtonClicked_calledTwice_notifiesDelegateOnlyOnce() {
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();

        var onAccept = model.get(EnterpriseSignalsDisclaimerProperties.ON_ACCEPT_CLICKED);
        onAccept.onClick(null);
        onAccept.onClick(null);

        verify(mDelegate, times(1)).onAccept();
    }

    @Test
    public void onCancelButtonClicked_calledTwice_notifiesDelegateOnlyOnce() {
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();

        var onCancel = model.get(EnterpriseSignalsDisclaimerProperties.ON_CANCEL_CLICKED);
        onCancel.onClick(null);
        onCancel.onClick(null);

        verify(mDelegate, times(1)).onDecline();
    }

    @Test
    public void onAcceptButtonClicked_thenCancelButtonClicked_ignoresSecondClick() {
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();

        model.get(EnterpriseSignalsDisclaimerProperties.ON_ACCEPT_CLICKED).onClick(null);
        model.get(EnterpriseSignalsDisclaimerProperties.ON_CANCEL_CLICKED).onClick(null);

        verify(mDelegate, times(1)).onAccept();
        verify(mDelegate, never()).onDecline();
    }

    @Test
    public void onCancelButtonClicked_thenAcceptButtonClicked_ignoresSecondClick() {
        EnterpriseSignalsDisclaimerMediator mediator =
                createMediatorForAccount(TestAccounts.MANAGED_ACCOUNT);
        PropertyModel model = mediator.getModel();

        model.get(EnterpriseSignalsDisclaimerProperties.ON_CANCEL_CLICKED).onClick(null);
        model.get(EnterpriseSignalsDisclaimerProperties.ON_ACCEPT_CLICKED).onClick(null);

        verify(mDelegate, times(1)).onDecline();
        verify(mDelegate, never()).onAccept();
    }
}
