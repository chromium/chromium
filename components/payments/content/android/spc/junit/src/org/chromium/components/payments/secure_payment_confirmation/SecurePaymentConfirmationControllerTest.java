// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.text.style.ClickableSpan;
import android.util.Pair;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationController.SpcResponseStatus;
import org.chromium.components.payments.ui.CurrencyFormatter;
import org.chromium.components.payments.ui.CurrencyFormatterJni;
import org.chromium.components.payments.ui.InputProtector;
import org.chromium.components.payments.ui.test_support.FakeClock;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.TextViewWithClickableSpans;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.lang.ref.WeakReference;
import java.util.Locale;

/** Unit tests for {@link SecurePaymentConfirmationController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {
            SecurePaymentConfirmationControllerTest.ShadowBottomSheetControllerProvider.class
        })
public class SecurePaymentConfirmationControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindow;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Callback<Integer> mResponseCallback;
    @Mock private SecurePaymentConfirmationBottomSheetObserver mBottomSheetObserver;

    private final FakeClock mClock = new FakeClock();

    private String mPayeeName;
    private Origin mPayeeOrigin;
    private String mPaymentInstrumentLabel;
    private PaymentItem mTotal;
    private Drawable mPaymentIcon;
    private Drawable mIssuerIcon;
    private Drawable mNetworkIcon;
    private String mRelyingPartyId;
    private SecurePaymentConfirmationController mController;

    /** The shadow of BottomSheetControllerProvider. Not to be used outside the test. */
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
        doReturn(new WeakReference<>(RuntimeEnvironment.getApplication()))
                .when(mWindow)
                .getContext();

        mBottomSheetController = mock(BottomSheetController.class);
        doReturn(true)
                .when(mBottomSheetController)
                .requestShowContent(any(BottomSheetContent.class), anyBoolean());
        ShadowBottomSheetControllerProvider.setBottomSheetController(mBottomSheetController);

        CurrencyFormatter.Natives currencyFormatter = mock(CurrencyFormatter.Natives.class);
        CurrencyFormatterJni.setInstanceForTesting(currencyFormatter);
        doReturn("$1.50")
                .when(currencyFormatter)
                .format(
                        /* nativeCurrencyFormatterAndroid= */ anyLong(),
                        any(CurrencyFormatter.class),
                        eq("1.50"));

        mPayeeName = "Payee Name";
        mPayeeOrigin = Origin.create(new GURL("https://test.payee"));
        mPaymentInstrumentLabel = "Payment Instrument Label";
        mTotal = new PaymentItem();
        mTotal.amount = new PaymentCurrencyAmount();
        mTotal.amount.currency = "CAD";
        mTotal.amount.value = "1.50";
        mPaymentIcon =
                new BitmapDrawable(
                        RuntimeEnvironment.getApplication().getResources(),
                        Bitmap.createBitmap(
                                new int[] {Color.RED},
                                /* width= */ 1,
                                /* height= */ 1,
                                Bitmap.Config.ARGB_8888));
        mIssuerIcon =
                new BitmapDrawable(
                        RuntimeEnvironment.getApplication().getResources(),
                        Bitmap.createBitmap(
                                new int[] {Color.GREEN},
                                /* width= */ 1,
                                /* height= */ 1,
                                Bitmap.Config.ARGB_8888));
        mNetworkIcon =
                new BitmapDrawable(
                        RuntimeEnvironment.getApplication().getResources(),
                        Bitmap.createBitmap(
                                new int[] {Color.BLUE},
                                /* width= */ 1,
                                /* height= */ 1,
                                Bitmap.Config.ARGB_8888));
        mRelyingPartyId = "Relying Party ID";
    }

    @Test
    public void testInit() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);
    }

    @Test
    public void testInit_withOptOut() {
        createController(/* showOptOut= */ true, /* informOnly= */ false);
    }

    @Test
    public void testInit_withInformOnly() {
        createController(/* showOptOut= */ false, /* informOnly= */ true);
    }

    @Test
    public void testInit_withOptOutAndInformOnly() {
        createController(/* showOptOut= */ true, /* informOnly= */ true);
    }

    @Test
    public void testInit_whenContextNull_throwsAssertError() {
        doReturn(new WeakReference<>(null)).when(mWindow).getContext();

        assertThrows(
                AssertionError.class,
                () -> createController(/* showOptOut= */ false, /* informOnly= */ false));
    }

    @Test
    public void testInit_whenBottomSheetControllerNull_throwsAssertError() {
        ShadowBottomSheetControllerProvider.setBottomSheetController(null);
        assertThrows(
                AssertionError.class,
                () -> createController(/* showOptOut= */ false, /* informOnly= */ false));
    }

    @Test
    public void testInitModel() {
        Context context = mWindow.getContext().get();
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        PropertyModel model = mController.getModelForTesting();

        assertTrue(model.get(SecurePaymentConfirmationProperties.SHOWS_ISSUER_NETWORK_ICONS));
        assertSame(mIssuerIcon, model.get(SecurePaymentConfirmationProperties.ISSUER_ICON));
        assertSame(mNetworkIcon, model.get(SecurePaymentConfirmationProperties.NETWORK_ICON));
        assertEquals(
                context.getString(
                        org.chromium.components.payments.R.string
                                .secure_payment_confirmation_verify_purchase),
                model.get(SecurePaymentConfirmationProperties.TITLE));
        assertEquals(
                String.format(
                        "%s (%s)",
                        mPayeeName,
                        UrlFormatter.formatOriginForSecurityDisplay(
                                mPayeeOrigin, SchemeDisplay.OMIT_HTTP_AND_HTTPS)),
                model.get(SecurePaymentConfirmationProperties.STORE_LABEL));
        assertEquals(
                Pair.create(mPaymentIcon, false),
                model.get(SecurePaymentConfirmationProperties.PAYMENT_ICON));
        assertEquals(
                mPaymentInstrumentLabel,
                model.get(SecurePaymentConfirmationProperties.PAYMENT_INSTRUMENT_LABEL));
        assertEquals(
                mTotal.amount.currency, model.get(SecurePaymentConfirmationProperties.CURRENCY));
        CurrencyFormatter formatter =
                new CurrencyFormatter(mTotal.amount.currency, Locale.getDefault());
        String totalAmountValue = formatter.format(mTotal.amount.value);
        formatter.destroy();
        assertEquals(totalAmountValue, model.get(SecurePaymentConfirmationProperties.TOTAL));
        assertNull(model.get(SecurePaymentConfirmationProperties.OPT_OUT_TEXT));
        assertSpannableStringsEqual(
                SpanApplier.applySpans(
                        context.getString(
                                org.chromium.components.payments.R.string
                                        .secure_payment_confirmation_footnote),
                        new SpanInfo("BEGIN_LINK", "END_LINK")),
                model.get(SecurePaymentConfirmationProperties.FOOTNOTE));
        assertEquals(
                context.getString(
                        org.chromium.components.payments.R.string.payments_continue_button),
                model.get(SecurePaymentConfirmationProperties.CONTINUE_BUTTON_LABEL));
    }

    @Test
    public void testInitModel_withOptOut() {
        Context context = mWindow.getContext().get();
        createController(/* showOptOut= */ true, /* informOnly= */ false);

        String deviceString =
                context.getString(
                        DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                                ? org.chromium.components.payments.R.string
                                        .secure_payment_confirmation_this_tablet_label
                                : org.chromium.components.payments.R.string
                                        .secure_payment_confirmation_this_phone_label);

        assertSpannableStringsEqual(
                SpanApplier.applySpans(
                        context.getString(
                                org.chromium.components.payments.R.string
                                        .secure_payment_confirmation_opt_out_label,
                                deviceString,
                                mRelyingPartyId),
                        new SpanInfo("BEGIN_LINK", "END_LINK")),
                mController
                        .getModelForTesting()
                        .get(SecurePaymentConfirmationProperties.OPT_OUT_TEXT));
    }

    @Test
    public void testInitModel_withInformOnly() {
        Context context = mWindow.getContext().get();
        createController(/* showOptOut= */ false, /* informOnly= */ true);
        PropertyModel model = mController.getModelForTesting();

        assertEquals(
                context.getString(
                        org.chromium.components.payments.R.string
                                .secure_payment_confirmation_inform_only_title),
                model.get(SecurePaymentConfirmationProperties.TITLE));
        assertNull(model.get(SecurePaymentConfirmationProperties.FOOTNOTE));
        assertEquals(
                context.getString(
                        org.chromium.components.payments.R.string.payments_confirm_button),
                model.get(SecurePaymentConfirmationProperties.CONTINUE_BUTTON_LABEL));
    }

    @Test
    public void testShow() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);

        assertTrue(mController.show());
        verify(mBottomSheetController)
                .requestShowContent(any(BottomSheetContent.class), anyBoolean());
        verify(mBottomSheetObserver).begin(eq(mController));
    }

    @Test
    public void testShow_whenRequestShowContentReturnsFalse() {
        doReturn(false)
                .when(mBottomSheetController)
                .requestShowContent(any(BottomSheetContent.class), anyBoolean());

        createController(/* showOptOut= */ false, /* informOnly= */ false);

        assertFalse(mController.show());
        verifyNoInteractions(mBottomSheetObserver);
    }

    @Test
    public void testHide() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        mController.hide();

        verify(mBottomSheetObserver).end();
        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), anyBoolean());
    }

    @Test
    public void testOnContinue() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        mController.getViewForTesting().mContinueButton.performClick();

        verify(mResponseCallback).onResult(eq(SpcResponseStatus.ACCEPT));
        // Verify hide() was called.
        verify(mBottomSheetObserver).end();
        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), anyBoolean());
    }

    @Test
    public void testOnContinue_withInformOnly() {
        createController(/* showOptOut= */ false, /* informOnly= */ true);
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        mController.getViewForTesting().mContinueButton.performClick();

        verify(mResponseCallback).onResult(eq(SpcResponseStatus.ANOTHER_WAY));
        // Verify hide() was called.
        verify(mBottomSheetObserver).end();
        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), anyBoolean());
    }

    @Test
    public void testOnContinue_whenInputThresholdNotReached() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        mController.getViewForTesting().mContinueButton.performClick();

        verifyNoInteractions(mResponseCallback);
        verifyNoInteractions(mBottomSheetObserver);
        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testOnCancel() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        mController.getViewForTesting().mCancelButton.performClick();

        verify(mResponseCallback).onResult(eq(SpcResponseStatus.CANCEL));
        // Verify hide() was called.
        verify(mBottomSheetObserver).end();
        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), anyBoolean());
    }

    @Test
    public void testOnCancel_whenInputThresholdNotReached() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        mController.getViewForTesting().mCancelButton.performClick();

        verifyNoInteractions(mResponseCallback);
        verifyNoInteractions(mBottomSheetObserver);
        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testOnVerifyAnotherWay() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        TextViewWithClickableSpans view = mController.getViewForTesting().mFootnote;
        ClickableSpan[] clickableSpans = view.getClickableSpans();
        assertEquals(1, clickableSpans.length);
        clickableSpans[0].onClick(view);

        verify(mResponseCallback).onResult(eq(SpcResponseStatus.ANOTHER_WAY));
        // Verify hide() was called.
        verify(mBottomSheetObserver).end();
        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), anyBoolean());
    }

    @Test
    public void testOnVerifyAnotherWay_whenInputThresholdNotReached() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        TextViewWithClickableSpans view = mController.getViewForTesting().mFootnote;
        ClickableSpan[] clickableSpans = view.getClickableSpans();
        assertEquals(1, clickableSpans.length);
        clickableSpans[0].onClick(view);

        verifyNoInteractions(mResponseCallback);
        verifyNoInteractions(mBottomSheetObserver);
        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testOnOptOut() {
        createController(/* showOptOut= */ true, /* informOnly= */ false);
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        TextViewWithClickableSpans view = mController.getViewForTesting().mOptOutText;
        ClickableSpan[] clickableSpans = view.getClickableSpans();
        assertEquals(1, clickableSpans.length);
        clickableSpans[0].onClick(view);

        verify(mResponseCallback).onResult(eq(SpcResponseStatus.OPT_OUT));
        // Verify hide() was called.
        verify(mBottomSheetObserver).end();
        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), anyBoolean());
    }

    @Test
    public void testOnOptOut_whenInputThresholdNotReached() {
        createController(/* showOptOut= */ true, /* informOnly= */ false);
        TextViewWithClickableSpans view = mController.getViewForTesting().mOptOutText;
        ClickableSpan[] clickableSpans = view.getClickableSpans();
        assertEquals(1, clickableSpans.length);
        clickableSpans[0].onClick(view);

        verifyNoInteractions(mResponseCallback);
        verifyNoInteractions(mBottomSheetObserver);
        verifyNoInteractions(mBottomSheetController);
    }

    private void createController(boolean showOptOut, boolean informOnly) {
        mController =
                new SecurePaymentConfirmationController(
                        mWindow,
                        mPayeeName,
                        mPayeeOrigin,
                        mPaymentInstrumentLabel,
                        mTotal,
                        mPaymentIcon,
                        mIssuerIcon,
                        mNetworkIcon,
                        mRelyingPartyId,
                        showOptOut,
                        informOnly,
                        mResponseCallback);
        InputProtector inputProtector = new InputProtector(mClock);
        inputProtector.markShowTime();
        mController.setInputProtectorForTesting(inputProtector);
        mController.setBottomSheetObserverForTesting(mBottomSheetObserver);
    }

    private void assertSpannableStringsEqual(
            SpannableString leftSpannableString, SpannableString rightSpannableString) {
        assertEquals(rightSpannableString.toString(), leftSpannableString.toString());
        assertEquals(rightSpannableString.length(), leftSpannableString.length());
        assertArrayEquals(
                leftSpannableString.getSpans(0, leftSpannableString.length(), SpanInfo.class),
                rightSpannableString.getSpans(0, rightSpannableString.length(), SpanInfo.class));
    }
}
