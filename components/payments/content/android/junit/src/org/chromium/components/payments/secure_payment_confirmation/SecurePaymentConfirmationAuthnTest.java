// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Answers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.payments.CurrencyFormatterJni;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.Origin;

import java.lang.ref.WeakReference;

/** A test for SecurePaymentConfirmationAuthn. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {SecurePaymentConfirmationAuthnTest.ShadowBottomSheetControllerProvider.class})
public class SecurePaymentConfirmationAuthnTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.WARN);
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock(answer = Answers.RETURNS_DEEP_STUBS)
    private WebContents mWebContents;

    private boolean mIsPaymentConfirmed;
    private boolean mIsPaymentCancelled;
    private Callback<Boolean> mCallback;

    private PaymentItem mTotal;
    private Drawable mDrawable;
    private SecurePaymentConfirmationAuthnController mAuthnController;

    /** The shadow of BottomSheetControllerProvider. Not to use outside the test. */
    @Implements(BottomSheetControllerProvider.class)
    /* package */ static class ShadowBottomSheetControllerProvider {
        private static BottomSheetController sBottomSheetController;

        @Implementation
        public static BottomSheetController from(WindowAndroid windowAndroid) {
            return sBottomSheetController;
        }

        private static void setBottomSheetController(BottomSheetController controller) {
            sBottomSheetController = controller;
        }
    }

    @Before
    public void setUp() {
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);
        setWindowAndroid(windowAndroid, mWebContents);
        Mockito.doReturn(new WeakReference<Context>(RuntimeEnvironment.application))
                .when(windowAndroid)
                .getContext();

        // Create formatter mocks
        UrlFormatter.Natives urlFormatterJniMock = Mockito.mock(UrlFormatter.Natives.class);
        CurrencyFormatter.Natives currencyFormatterJniMock =
                Mockito.mock(CurrencyFormatter.Natives.class);
        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, urlFormatterJniMock);
        mJniMocker.mock(CurrencyFormatterJni.TEST_HOOKS, currencyFormatterJniMock);
        Mockito.doReturn("example.com")
                .when(urlFormatterJniMock)
                .formatUrlForDisplayOmitScheme(Mockito.anyString());
        Mockito.doReturn("$1.00")
                .when(currencyFormatterJniMock)
                .format(Mockito.anyLong(), Mockito.any(CurrencyFormatter.class),
                        Mockito.anyString());

        mDrawable = Mockito.mock(Drawable.class);
        mTotal = new PaymentItem();
        mTotal.amount = new PaymentCurrencyAmount();
        mTotal.amount.currency = "USD";
        mTotal.amount.value = "1.00";
        mCallback = (response) -> {
            if (response) {
                mIsPaymentConfirmed = true;
            } else {
                mIsPaymentCancelled = true;
            }
        };

        ShadowBottomSheetControllerProvider.setBottomSheetController(
                createBottomSheetController(/*requestShowContentResponse=*/true));
    }

    @After
    public void tearDown() {
        if (mAuthnController != null) mAuthnController.hide();
    }

    private void createAuthnController() {
        mAuthnController = SecurePaymentConfirmationAuthnController.create(mWebContents);
    }

    private BottomSheetController createBottomSheetController(boolean requestShowContentResponse) {
        BottomSheetController controller = Mockito.mock(BottomSheetController.class);
        Mockito.doReturn(requestShowContentResponse)
                .when(controller)
                .requestShowContent(Mockito.any(BottomSheetContent.class), Mockito.anyBoolean());
        return controller;
    }

    private boolean show() {
        if (mAuthnController == null) return false;

        mIsPaymentConfirmed = false;
        mIsPaymentCancelled = false;
        org.chromium.url.internal.mojom.Origin mojoOrigin =
                new org.chromium.url.internal.mojom.Origin();
        return mAuthnController.show(
                mDrawable, "paymentInstrumentLabel", mTotal, mCallback, new Origin(mojoOrigin));
    }

    private void setWindowAndroid(WindowAndroid windowAndroid, WebContents webContents) {
        Mockito.doReturn(windowAndroid).when(webContents).getTopLevelNativeWindow();
    }

    private void setContext(Context context) {
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        Mockito.doReturn(new WeakReference<Context>(context)).when(windowAndroid).getContext();
    }

    @Test
    @Feature({"Payments"})
    public void testOnAuthnConfirmation() {
        createAuthnController();
        show();
        mAuthnController.getView().mContinueButton.performClick();
        Assert.assertTrue(mIsPaymentConfirmed);
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testOnAuthnCancellation() {
        createAuthnController();
        show();
        mAuthnController.getView().mCancelButton.performClick();
        Assert.assertTrue(mIsPaymentCancelled);
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testHide() {
        createAuthnController();
        show();
        mAuthnController.hide();
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testRequestShowContentFalse() {
        createAuthnController();
        ShadowBottomSheetControllerProvider.setBottomSheetController(
                createBottomSheetController(/*requestShowContentResponse=*/false));
        Assert.assertFalse(show());
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testCreateWithNullWebContents() {
        mWebContents = null;
        createAuthnController();
        Assert.assertNull(mAuthnController);
    }

    @Test
    @Feature({"Payments"})
    public void testShowWithNullWindowAndroid() {
        setWindowAndroid(null, mWebContents);
        createAuthnController();
        Assert.assertFalse(show());
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testShowWithNullContext() {
        setContext(null);
        createAuthnController();
        Assert.assertFalse(show());
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testShowWithNullBottomSheetController() {
        ShadowBottomSheetControllerProvider.setBottomSheetController(null);
        createAuthnController();
        Assert.assertFalse(show());
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testShowTwiceWithHide() {
        createAuthnController();
        Assert.assertTrue(show());
        mAuthnController.hide();
        Assert.assertTrue(show());
    }

    @Test
    @Feature({"Payments"})
    public void testShowTwiceWithoutHide() {
        createAuthnController();
        Assert.assertTrue(show());
        Assert.assertFalse(show());
    }
}
