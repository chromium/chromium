// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
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
import android.view.View;
import android.widget.ScrollView;

import androidx.core.graphics.drawable.RoundedBitmapDrawable;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiContainer;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiController;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiControllerImpl;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiEntry;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiScope;
import org.chromium.components.browser_ui.adaptive_ui.DismissReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.UserCriticalFeature;
import org.chromium.components.payments.PaymentApp.PaymentEntityLogo;
import org.chromium.components.payments.SPCTransactionMode;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationController.AdaptiveUiPresenter;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationController.BottomSheetPresenter;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationController.SpcResponseStatus;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationProperties.ItemProperties;
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
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.TextViewWithClickableSpans;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.lang.ref.WeakReference;
import java.time.Duration;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/** Unit tests for {@link SecurePaymentConfirmationController} */
@RunWith(BaseRobolectricTestRunner.class)
public class SecurePaymentConfirmationControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    static class TestPaymentEntityLogo implements PaymentEntityLogo {
        private final Bitmap mIcon;
        private final String mLabel;

        TestPaymentEntityLogo(Bitmap icon, String label) {
            mIcon = icon;
            mLabel = label;
        }

        @Override
        public String getLabel() {
            return mLabel;
        }

        @Override
        public Bitmap getIcon() {
            return mIcon;
        }
    }

    /**
     * Extends {@link FakeModalDialogManager} to notify registered {@link
     * ModalDialogManagerObserver}s when a dialog is shown immediately, or hold the model in a
     * pending set when simulating a queued presentation.
     */
    private static class TestModalDialogManager extends FakeModalDialogManager {
        private final List<ModalDialogManagerObserver> mObservers = new ArrayList<>();
        private final Set<PropertyModel> mPendingModels = new HashSet<>();
        private boolean mQueueNextDialog;
        private int mLastDismissalCause;

        TestModalDialogManager() {
            super(ModalDialogType.TAB);
        }

        void setQueueNextDialog(boolean queueNextDialog) {
            mQueueNextDialog = queueNextDialog;
        }

        int getLastDismissalCause() {
            return mLastDismissalCause;
        }

        @Override
        public void addObserver(ModalDialogManagerObserver observer) {
            super.addObserver(observer);
            mObservers.add(observer);
        }

        @Override
        public void removeObserver(ModalDialogManagerObserver observer) {
            super.removeObserver(observer);
            mObservers.remove(observer);
        }

        @Override
        public void showDialog(PropertyModel model, int dialogType) {
            if (mQueueNextDialog) {
                mPendingModels.add(model);
                return;
            }
            super.showDialog(model, dialogType);
            for (ModalDialogManagerObserver observer : new ArrayList<>(mObservers)) {
                observer.onDialogCreated(model, /* dialog= */ null);
                observer.onDialogShown(new View(RuntimeEnvironment.getApplication()));
            }
        }

        @Override
        public void dismissDialog(PropertyModel model, int dismissalCause) {
            if (mPendingModels.remove(model)) {
                mLastDismissalCause = dismissalCause;
                model.get(ModalDialogProperties.CONTROLLER).onDismiss(model, dismissalCause);
                return;
            }
            if (getShownDialogModel() != model) {
                return;
            }
            mLastDismissalCause = dismissalCause;
            super.dismissDialog(model, dismissalCause);
        }
    }

    @Mock private WindowAndroid mWindow;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Callback<Integer> mResponseCallback;
    @Mock private SecurePaymentConfirmationBottomSheetObserver mBottomSheetObserver;

    private final FakeClock mClock = new FakeClock();
    private final UnownedUserDataHost mWindowUserDataHost = new UnownedUserDataHost();
    private final List<Integer> mAdaptiveResponses = new ArrayList<>();

    private List<PaymentEntityLogo> mPaymentEntityLogos;
    private String mPayeeName;
    private Origin mPayeeOrigin;
    private String mPaymentInstrumentLabelPrimary;
    private String mPaymentInstrumentLabelSecondary;
    private PaymentItem mTotal;
    private Drawable mPaymentIcon;
    private String mRelyingPartyId;
    private SecurePaymentConfirmationController mController;
    private AdaptiveUiControllerImpl mAdaptiveUiController;
    private TestModalDialogManager mModalDialogManager;

    @Before
    public void setUp() {
        doReturn(new WeakReference<>(RuntimeEnvironment.getApplication()))
                .when(mWindow)
                .getContext();
        doReturn(mWindowUserDataHost).when(mWindow).getUnownedUserDataHost();

        mBottomSheetController = mock(BottomSheetController.class);
        doReturn(true)
                .when(mBottomSheetController)
                .requestShowContent(any(BottomSheetContent.class), anyBoolean());
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);

        CurrencyFormatter.Natives currencyFormatter = mock(CurrencyFormatter.Natives.class);
        CurrencyFormatterJni.setInstanceForTesting(currencyFormatter);
        doReturn("$1.50")
                .when(currencyFormatter)
                .format(/* nativeCurrencyFormatterAndroid= */ anyLong(), eq("1.50"));

        mPaymentEntityLogos =
                Arrays.asList(
                        new TestPaymentEntityLogo(
                                Bitmap.createBitmap(
                                        new int[] {Color.GREEN},
                                        /* width= */ 1,
                                        /* height= */ 1,
                                        Bitmap.Config.ARGB_8888),
                                "first logo label"),
                        new TestPaymentEntityLogo(
                                Bitmap.createBitmap(
                                        new int[] {Color.BLUE},
                                        /* width= */ 1,
                                        /* height= */ 1,
                                        Bitmap.Config.ARGB_8888),
                                "second logo label"));
        mPayeeName = "Payee Name";
        mPayeeOrigin = Origin.create(new GURL("https://test.payee"));
        mPaymentInstrumentLabelPrimary = "Payment Instrument Label Primary";
        mPaymentInstrumentLabelSecondary = "Payment Instrument Label Secondary";
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
        mRelyingPartyId = "Relying Party ID";
    }

    @After
    public void tearDown() {
        if (mAdaptiveUiController != null) {
            mAdaptiveUiController.destroy();
            mAdaptiveUiController = null;
        }
        mWindowUserDataHost.destroy();
        DeviceInfo.resetIsDesktopForTesting();
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
        BottomSheetControllerProvider.setInstanceForTesting(null);
        assertThrows(
                AssertionError.class,
                () -> createController(/* showOptOut= */ false, /* informOnly= */ false));
    }

    @Test
    public void testInitModel() {
        Context context = RuntimeEnvironment.getApplication();
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        PropertyModel model = mController.getModelForTesting();

        assertSame(
                mPaymentEntityLogos, model.get(SecurePaymentConfirmationProperties.HEADER_LOGOS));
        assertEquals(
                context.getString(R.string.secure_payment_confirmation_title),
                model.get(SecurePaymentConfirmationProperties.TITLE));
        ModelList itemList =
                model.get(SecurePaymentConfirmationProperties.ITEM_LIST_ADAPTER).getModelList();
        assertEquals(3, itemList.size());
        ListItem storeItem = itemList.get(0);
        assertEquals(
                context.getString(R.string.secure_payment_confirmation_store_label),
                storeItem.model.get(ItemProperties.ICON_LABEL));
        assertEquals(mPayeeName, storeItem.model.get(ItemProperties.PRIMARY_TEXT));
        assertEquals(
                UrlFormatter.formatOriginForSecurityDisplay(
                        mPayeeOrigin, SchemeDisplay.OMIT_HTTP_AND_HTTPS),
                storeItem.model.get(ItemProperties.SECONDARY_TEXT));
        ListItem paymentItem = itemList.get(1);
        assertEquals(
                ((BitmapDrawable) mPaymentIcon).getBitmap(),
                ((RoundedBitmapDrawable) paymentItem.model.get(ItemProperties.ICON)).getBitmap());
        assertEquals(
                mPaymentInstrumentLabelPrimary, paymentItem.model.get(ItemProperties.PRIMARY_TEXT));
        assertEquals(
                mPaymentInstrumentLabelSecondary,
                paymentItem.model.get(ItemProperties.SECONDARY_TEXT));
        ListItem totalItem = itemList.get(2);
        assertEquals(
                context.getString(R.string.secure_payment_confirmation_total_label),
                totalItem.model.get(ItemProperties.ICON_LABEL));
        CurrencyFormatter formatter =
                new CurrencyFormatter(mTotal.amount.currency, Locale.getDefault());
        String totalAmountValue = formatter.format(mTotal.amount.value);
        formatter.destroy();
        assertEquals(
                String.format("%s %s", mTotal.amount.currency, totalAmountValue),
                totalItem.model.get(ItemProperties.PRIMARY_TEXT));
        assertNull(model.get(SecurePaymentConfirmationProperties.OPT_OUT_TEXT));
        assertSpannableStringsEqual(
                SpanApplier.applySpans(
                        context.getString(R.string.secure_payment_confirmation_footnote),
                        new SpanInfo("BEGIN_LINK", "END_LINK")),
                model.get(SecurePaymentConfirmationProperties.FOOTNOTE));
        assertEquals(
                context.getString(R.string.secure_payment_confirmation_verify_button_label),
                model.get(SecurePaymentConfirmationProperties.CONTINUE_BUTTON_LABEL));
    }

    @Test
    public void testInitModel_withOptOut() {
        Context context = RuntimeEnvironment.getApplication();
        createController(/* showOptOut= */ true, /* informOnly= */ false);

        String deviceString =
                context.getString(
                        DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                                ? R.string.secure_payment_confirmation_this_tablet_label
                                : R.string.secure_payment_confirmation_this_phone_label);

        assertSpannableStringsEqual(
                SpanApplier.applySpans(
                        context.getString(
                                R.string.secure_payment_confirmation_opt_out_label,
                                deviceString,
                                mRelyingPartyId),
                        new SpanInfo("BEGIN_LINK", "END_LINK")),
                mController
                        .getModelForTesting()
                        .get(SecurePaymentConfirmationProperties.OPT_OUT_TEXT));
    }

    @Test
    public void testInitModel_withInformOnly() {
        Context context = RuntimeEnvironment.getApplication();
        createController(/* showOptOut= */ false, /* informOnly= */ true);
        PropertyModel model = mController.getModelForTesting();

        assertEquals(
                context.getString(R.string.secure_payment_confirmation_inform_only_title),
                model.get(SecurePaymentConfirmationProperties.TITLE));
        assertNull(model.get(SecurePaymentConfirmationProperties.FOOTNOTE));
        assertEquals(
                context.getString(R.string.payments_confirm_button),
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
    public void testShow_whenCalledTwice_returnsFalse() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        assertTrue(mController.show());
        clearInvocations(mBottomSheetObserver, mBottomSheetController);

        assertFalse(mController.show());
        verifyNoInteractions(mBottomSheetObserver);
        verifyNoInteractions(mBottomSheetController);
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
    public void testHide_afterFailedShow_removesQueuedContent() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        doReturn(false)
                .when(mBottomSheetController)
                .requestShowContent(any(BottomSheetContent.class), anyBoolean());
        assertFalse(mController.show());

        mController.hide();

        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), anyBoolean());
        verifyNoInteractions(mResponseCallback);
    }

    @Test
    public void testHide() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        assertTrue(mController.show());
        mController.hide();

        verify(mBottomSheetObserver).end();
        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), anyBoolean());
    }

    @Test
    public void testOnContinue() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        assertTrue(mController.show());
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
        assertTrue(mController.show());
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
        assertTrue(mController.show());
        clearInvocations(mBottomSheetObserver, mBottomSheetController);
        mController.getViewForTesting().mContinueButton.performClick();

        verifyNoInteractions(mResponseCallback);
        verifyNoInteractions(mBottomSheetObserver);
        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testOnContinue_ignoresSecondClick() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        assertTrue(mController.show());
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        mController.getViewForTesting().mContinueButton.performClick();
        mController.getViewForTesting().mContinueButton.performClick();

        verify(mResponseCallback).onResult(eq(SpcResponseStatus.ACCEPT));
        verify(mBottomSheetObserver).end();
        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), anyBoolean());
    }

    @Test
    public void testOnCancel() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        assertTrue(mController.show());
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        mController.onCancel();

        verify(mResponseCallback).onResult(eq(SpcResponseStatus.CANCEL));
        // Verify hide() was called.
        verify(mBottomSheetObserver).end();
        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), anyBoolean());
    }

    @Test
    public void testOnCancel_whenInputThresholdNotReached() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        assertTrue(mController.show());
        clearInvocations(mBottomSheetObserver, mBottomSheetController);
        mController.onCancel();

        verifyNoInteractions(mResponseCallback);
        verifyNoInteractions(mBottomSheetObserver);
        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testOnVerifyAnotherWay() {
        createController(/* showOptOut= */ false, /* informOnly= */ false);
        assertTrue(mController.show());
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
        assertTrue(mController.show());
        clearInvocations(mBottomSheetObserver, mBottomSheetController);
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
        assertTrue(mController.show());
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
        assertTrue(mController.show());
        clearInvocations(mBottomSheetObserver, mBottomSheetController);
        TextViewWithClickableSpans view = mController.getViewForTesting().mOptOutText;
        ClickableSpan[] clickableSpans = view.getClickableSpans();
        assertEquals(1, clickableSpans.length);
        clickableSpans[0].onClick(view);

        verifyNoInteractions(mResponseCallback);
        verifyNoInteractions(mBottomSheetObserver);
        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testTestHelpers_afterResponse_returnFalse() {
        createController(/* showOptOut= */ true, /* informOnly= */ false);
        assertTrue(mController.show());
        assertTrue(mController.cancelForTest());
        verify(mResponseCallback).onResult(eq(SpcResponseStatus.CANCEL));
        clearInvocations((Object) mResponseCallback);

        assertFalse(mController.cancelForTest());
        assertFalse(mController.optOutForTest());
        verifyNoInteractions(mResponseCallback);
    }

    @Test
    public void testTransactionMode_autoAccept() {
        createController(
                /* showOptOut= */ false, /* informOnly= */ false, SPCTransactionMode.AUTO_ACCEPT);
        assertTrue(mController.show());

        // The automation should run immediately when show is called, and accept the prompt.
        verify(mResponseCallback).onResult(eq(SpcResponseStatus.ACCEPT));
    }

    @Test
    public void testTransactionMode_autoAccept_withInformOnly() {
        createController(
                /* showOptOut= */ false, /* informOnly= */ true, SPCTransactionMode.AUTO_ACCEPT);
        assertTrue(mController.show());

        // The automation should run immediately when show is called, and accept the prompt. For the
        // inform prompt, this is equivalent to verify another way.
        verify(mResponseCallback).onResult(eq(SpcResponseStatus.ANOTHER_WAY));
    }

    @Test
    public void testTransactionMode_autoVerifyAnotherWay() {
        createController(
                /* showOptOut= */ false,
                /* informOnly= */ false,
                SPCTransactionMode.AUTO_AUTH_ANOTHER_WAY);
        assertTrue(mController.show());

        // The automation should run immediately when show is called, and choose to verify another
        // way.
        verify(mResponseCallback).onResult(eq(SpcResponseStatus.ANOTHER_WAY));
    }

    @Test
    public void testTransactionMode_autoVerifyAnotherWay_withInformOnly() {
        createController(
                /* showOptOut= */ false,
                /* informOnly= */ true,
                SPCTransactionMode.AUTO_AUTH_ANOTHER_WAY);
        assertTrue(mController.show());

        // The automation should run immediately when show is called, and choose to verify another
        // way.
        verify(mResponseCallback).onResult(eq(SpcResponseStatus.ANOTHER_WAY));
    }

    @Test
    public void testTransactionMode_autoReject() {
        createController(
                /* showOptOut= */ false, /* informOnly= */ false, SPCTransactionMode.AUTO_REJECT);
        assertTrue(mController.show());

        // The automation should run immediately when show is called, and reject the prompt.
        verify(mResponseCallback).onResult(eq(SpcResponseStatus.CANCEL));
    }

    @Test
    public void testTransactionMode_autoReject_withInformOnly() {
        createController(
                /* showOptOut= */ false, /* informOnly= */ true, SPCTransactionMode.AUTO_REJECT);
        assertTrue(mController.show());

        // The automation should run immediately when show is called, and reject the prompt.
        verify(mResponseCallback).onResult(eq(SpcResponseStatus.CANCEL));
    }

    @Test
    public void testTransactionMode_autoOptOut() {
        createController(
                /* showOptOut= */ true, /* informOnly= */ false, SPCTransactionMode.AUTO_OPT_OUT);
        assertTrue(mController.show());

        // The automation should run immediately when show is called, and opt out of the prompt.
        verify(mResponseCallback).onResult(eq(SpcResponseStatus.OPT_OUT));
    }

    @Test
    public void testTransactionMode_autoOptOut_withInformOnly() {
        createController(
                /* showOptOut= */ true, /* informOnly= */ true, SPCTransactionMode.AUTO_OPT_OUT);
        assertTrue(mController.show());

        // The automation should run immediately when show is called, and opt out of the prompt.
        verify(mResponseCallback).onResult(eq(SpcResponseStatus.OPT_OUT));
    }

    // ---------------------------------------------------------------------------------------------
    // Adaptive UI tests.
    // ---------------------------------------------------------------------------------------------

    @Test
    public void testAdaptiveUi_whenControllerAbsent_usesLegacyBottomSheet() {
        assertNull(AdaptiveUiController.from(mWindow));
        createController(/* showOptOut= */ false, /* informOnly= */ false);

        assertTrue(mController.getPresenterForTesting() instanceof BottomSheetPresenter);
        assertEquals(View.VISIBLE, mController.getViewForTesting().mContinueButton.getVisibility());
        assertTrue(mController.show());
        verify(mBottomSheetController)
                .requestShowContent(any(BottomSheetContent.class), anyBoolean());
    }

    @Test
    public void testAdaptiveUi_whenControllerAbsent_keepsLegacyPadding() {
        Context context = RuntimeEnvironment.getApplication();
        int largePadding =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.secure_payment_confirmation_ui_large_padding);
        int smallPadding =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.secure_payment_confirmation_ui_small_padding);
        createController(/* showOptOut= */ false, /* informOnly= */ false);

        View legacyBody = mController.getViewForTesting().mContentView.getChildAt(0);
        assertNotNull(legacyBody);
        assertEquals(largePadding, legacyBody.getPaddingStart());
        assertEquals(smallPadding, legacyBody.getPaddingTop());
        assertEquals(largePadding, legacyBody.getPaddingEnd());
        assertEquals(largePadding, legacyBody.getPaddingBottom());
    }

    @Test
    public void testAdaptiveUi_entryConfiguration() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);

        AdaptiveUiEntry entry = adaptiveEntry();
        assertNotNull(entry);
        assertEquals(AdaptiveUiScope.TRANSIENT, entry.getScope());
        assertEquals(
                Set.of(AdaptiveUiContainer.BOTTOM_SHEET, AdaptiveUiContainer.MODAL_DIALOG),
                entry.getAllowedContainers());
        assertEquals(
                UserCriticalFeature.SECURE_PAYMENT_CONFIRMATION,
                entry.getPropertyModel().get(AdaptiveUiProperties.SHEET_USER_CRITICAL));
        assertNotNull(entry.getPropertyModel().get(AdaptiveUiProperties.DELEGATE));
    }

    @Test
    public void testAdaptiveUi_modelCustomViewAndHiddenLegacyButton() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);

        AdaptiveUiEntry entry = adaptiveEntry();
        assertNotNull(entry);
        View customView = entry.getPropertyModel().get(AdaptiveUiProperties.CUSTOM_VIEW);
        assertNotNull(customView);
        assertFalse(customView instanceof ScrollView);
        assertSame(customView, mController.getViewForTesting().mTitle.getParent());
        assertEquals(0, mController.getViewForTesting().mContentView.getChildCount());
        assertEquals(View.GONE, mController.getViewForTesting().mContinueButton.getVisibility());
    }

    @Test
    public void testAdaptiveUi_customViewClearsBottomPadding() {
        Context context = RuntimeEnvironment.getApplication();
        int largePadding =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.secure_payment_confirmation_ui_large_padding);
        int smallPadding =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.secure_payment_confirmation_ui_small_padding);
        attachAdaptiveUiController(/* isDesktop= */ false);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);

        View customView = adaptiveEntry().getPropertyModel().get(AdaptiveUiProperties.CUSTOM_VIEW);
        assertNotNull(customView);
        assertEquals(largePadding, customView.getPaddingStart());
        assertEquals(smallPadding, customView.getPaddingTop());
        assertEquals(largePadding, customView.getPaddingEnd());
        assertEquals(0, customView.getPaddingBottom());
    }

    @Test
    public void testAdaptiveUi_modelProperties() {
        Context context = RuntimeEnvironment.getApplication();
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);

        PropertyModel adaptiveModel = adaptiveEntry().getPropertyModel();
        assertNull(adaptiveModel.get(AdaptiveUiProperties.TITLE));
        assertEquals(
                context.getString(
                        R.string.secure_payment_confirmation_authentication_sheet_description),
                adaptiveModel.get(AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION));
        assertEquals(
                R.string.secure_payment_confirmation_authentication_sheet_opened,
                adaptiveModel.get(AdaptiveUiProperties.SHEET_OPENED_ACCESSIBILITY_STRING_ID));
        assertEquals(
                R.string.secure_payment_confirmation_authentication_sheet_closed,
                adaptiveModel.get(AdaptiveUiProperties.SHEET_CLOSED_ACCESSIBILITY_STRING_ID));
        assertTrue(adaptiveModel.get(AdaptiveUiProperties.SHOW_SHEET_HANDLEBAR));
        assertEquals(
                context.getString(R.string.secure_payment_confirmation_verify_button_label),
                adaptiveModel.get(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT));
        assertNotNull(adaptiveModel.get(AdaptiveUiProperties.DELEGATE));
        assertEquals(
                InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD,
                adaptiveModel.get(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS));
    }

    @Test
    public void testAdaptiveUi_modelPrimaryButtonText_withInformOnly() {
        Context context = RuntimeEnvironment.getApplication();
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ true);

        PropertyModel adaptiveModel = adaptiveEntry().getPropertyModel();
        assertEquals(
                context.getString(R.string.payments_confirm_button),
                adaptiveModel.get(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT));
    }

    @Test
    public void testAdaptiveUi_dialogPrimaryClick_acceptsOnceAndDismissesAsPrimary() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);
        HistogramWatcher dismissWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.AdaptiveUi.DismissReason.ModalDialog",
                                DismissReason.PRIMARY_ACTION)
                        .build();

        assertTrue(mController.show());
        assertNotNull(mModalDialogManager.getShownDialogModel());
        mModalDialogManager.clickPositiveButton();

        assertEquals(List.of(SpcResponseStatus.ACCEPT), mAdaptiveResponses);
        assertNull(mModalDialogManager.getShownDialogModel());
        assertEquals(
                DialogDismissalCause.POSITIVE_BUTTON_CLICKED,
                mModalDialogManager.getLastDismissalCause());
        dismissWatcher.assertExpected();
    }

    @Test
    public void testAdaptiveUi_dialogPrimaryClickWithInformOnly_sendsAnotherWayOnce() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ true);
        HistogramWatcher dismissWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.AdaptiveUi.DismissReason.ModalDialog",
                                DismissReason.PRIMARY_ACTION)
                        .build();

        assertTrue(mController.show());
        mModalDialogManager.clickPositiveButton();

        assertEquals(List.of(SpcResponseStatus.ANOTHER_WAY), mAdaptiveResponses);
        assertNull(mModalDialogManager.getShownDialogModel());
        assertEquals(
                DialogDismissalCause.POSITIVE_BUTTON_CLICKED,
                mModalDialogManager.getLastDismissalCause());
        dismissWatcher.assertExpected();
    }

    @Test
    public void testAdaptiveUi_dialogPrimaryClick_ignoresSubsequentDismissAndLinkTap() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);
        assertTrue(mController.show());

        mModalDialogManager.clickPositiveButton();
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        TextViewWithClickableSpans footnoteView = mController.getViewForTesting().mFootnote;
        ClickableSpan[] spans = footnoteView.getClickableSpans();
        assertEquals(1, spans.length);
        spans[0].onClick(footnoteView);

        assertEquals(List.of(SpcResponseStatus.ACCEPT), mAdaptiveResponses);
    }

    @Test
    public void testAdaptiveUi_dialogCloseButton_cancelsWithinInputProtectionWindow() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);
        HistogramWatcher dismissWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.AdaptiveUi.DismissReason.ModalDialog",
                                DismissReason.CLOSE_BUTTON)
                        .build();

        assertTrue(mController.show());
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        assertNotNull(dialogModel);
        dialogModel
                .get(ModalDialogProperties.TITLE_CLOSE_BUTTON_CLICK_LISTENER)
                .onClick(new View(RuntimeEnvironment.getApplication()));

        assertEquals(List.of(SpcResponseStatus.CANCEL), mAdaptiveResponses);
        assertNull(mModalDialogManager.getShownDialogModel());
        dismissWatcher.assertExpected();
    }

    @Test
    public void testAdaptiveUi_dialogBackPress_cancelsWithinInputProtectionWindow() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);
        HistogramWatcher dismissWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.AdaptiveUi.DismissReason.ModalDialog",
                                DismissReason.BACK_PRESS)
                        .build();

        assertTrue(mController.show());
        mModalDialogManager.dismissDialog(
                mModalDialogManager.getShownDialogModel(), DialogDismissalCause.NAVIGATE_BACK);

        assertEquals(List.of(SpcResponseStatus.CANCEL), mAdaptiveResponses);
        assertNull(mModalDialogManager.getShownDialogModel());
        dismissWatcher.assertExpected();
    }

    @Test
    public void testAdaptiveUi_dialogTouchOutside_cancelsWithinInputProtectionWindow() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);
        HistogramWatcher dismissWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.AdaptiveUi.DismissReason.ModalDialog",
                                DismissReason.TOUCH_OUTSIDE)
                        .build();

        assertTrue(mController.show());
        mModalDialogManager.dismissDialog(
                mModalDialogManager.getShownDialogModel(), DialogDismissalCause.TOUCH_OUTSIDE);

        assertEquals(List.of(SpcResponseStatus.CANCEL), mAdaptiveResponses);
        assertNull(mModalDialogManager.getShownDialogModel());
        dismissWatcher.assertExpected();
    }

    @Test
    public void testAdaptiveUi_sheetPrimaryClick_acceptsOnce() {
        attachAdaptiveUiController(/* isDesktop= */ false);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);
        HistogramWatcher dismissWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.AdaptiveUi.DismissReason.BottomSheet",
                                DismissReason.PRIMARY_ACTION)
                        .build();

        BottomSheetContent sheetContent = showAdaptiveSheet();
        ShadowSystemClock.advanceBy(
                Duration.ofMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD + 1));
        View primaryButton =
                sheetContent.getContentView().findViewById(R.id.adaptive_ui_sheet_primary_button);
        assertNotNull(primaryButton);
        primaryButton.performClick();

        verify(mBottomSheetController)
                .hideContent(
                        sheetContent, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
        sheetContent.destroy();

        assertEquals(List.of(SpcResponseStatus.ACCEPT), mAdaptiveResponses);
        assertNull(mModalDialogManager.getShownDialogModel());
        dismissWatcher.assertExpected();
    }

    @Test
    public void testAdaptiveUi_sheetDismiss_cancelsOnce() {
        attachAdaptiveUiController(/* isDesktop= */ false);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);
        HistogramWatcher dismissWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.AdaptiveUi.DismissReason.BottomSheet",
                                DismissReason.BACK_PRESS)
                        .build();

        BottomSheetContent sheetContent = showAdaptiveSheet();
        sheetContent.onBackPressed();

        verify(mBottomSheetController)
                .hideContent(sheetContent, /* animate= */ true, StateChangeReason.BACK_PRESS);
        sheetContent.destroy();

        assertEquals(List.of(SpcResponseStatus.CANCEL), mAdaptiveResponses);
        assertNull(mModalDialogManager.getShownDialogModel());
        dismissWatcher.assertExpected();
    }

    @Test
    public void testAdaptiveUi_hide_dismissesAsFeatureWithNoResponse() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);
        HistogramWatcher dismissWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.AdaptiveUi.DismissReason.ModalDialog",
                                DismissReason.DISMISSED_BY_FEATURE)
                        .build();

        assertTrue(mController.show());
        mController.hide();

        assertTrue(mAdaptiveResponses.isEmpty());
        assertNull(mModalDialogManager.getShownDialogModel());
        assertEquals(
                DialogDismissalCause.DISMISSED_BY_NATIVE,
                mModalDialogManager.getLastDismissalCause());
        dismissWatcher.assertExpected();
    }

    @Test
    public void testAdaptiveUi_hide_ignoresSubsequentClose() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);
        assertTrue(mController.show());
        mController.hide();

        mController.hide();
        adaptiveEntry()
                .getPropertyModel()
                .get(AdaptiveUiProperties.DELEGATE)
                .onDismissed(DismissReason.CLOSE_BUTTON);
        assertTrue(mAdaptiveResponses.isEmpty());
    }

    @Test
    public void testAdaptiveUi_inputProtectorStartsOnShown() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ true, /* informOnly= */ false);
        InputProtector inputProtector = new InputProtector(mClock);
        inputProtector.markShowTime();
        mController.setInputProtectorForTesting(inputProtector);
        mClock.advanceCurrentTimeMillis(1000);

        assertTrue(mController.show());

        TextViewWithClickableSpans optOutView = mController.getViewForTesting().mOptOutText;
        ClickableSpan[] spans = optOutView.getClickableSpans();
        assertEquals(1, spans.length);

        mClock.advanceCurrentTimeMillis(200);
        spans[0].onClick(optOutView);
        assertTrue(mAdaptiveResponses.isEmpty());
        assertNotNull(mModalDialogManager.getShownDialogModel());

        mClock.advanceCurrentTimeMillis(300);
        spans[0].onClick(optOutView);
        assertEquals(List.of(SpcResponseStatus.OPT_OUT), mAdaptiveResponses);
        assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void testAdaptiveUi_onVerifyAnotherWay_beforeInputThreshold_isIgnored() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);
        assertTrue(mController.show());

        TextViewWithClickableSpans footnoteView = mController.getViewForTesting().mFootnote;
        ClickableSpan[] spans = footnoteView.getClickableSpans();
        assertEquals(1, spans.length);
        spans[0].onClick(footnoteView);

        assertTrue(mAdaptiveResponses.isEmpty());
        assertNotNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void testAdaptiveUi_onVerifyAnotherWay_afterInputThreshold_sendsAnotherWayOnce() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);
        HistogramWatcher dismissWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.AdaptiveUi.DismissReason.ModalDialog",
                                DismissReason.DISMISSED_BY_FEATURE)
                        .build();
        assertTrue(mController.show());

        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        TextViewWithClickableSpans footnoteView = mController.getViewForTesting().mFootnote;
        ClickableSpan[] spans = footnoteView.getClickableSpans();
        assertEquals(1, spans.length);
        spans[0].onClick(footnoteView);

        assertEquals(List.of(SpcResponseStatus.ANOTHER_WAY), mAdaptiveResponses);
        assertNull(mModalDialogManager.getShownDialogModel());
        dismissWatcher.assertExpected();
    }

    @Test
    public void testAdaptiveUi_onOptOut_beforeInputThreshold_isIgnored() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ true, /* informOnly= */ false);
        assertTrue(mController.show());

        TextViewWithClickableSpans optOutView = mController.getViewForTesting().mOptOutText;
        ClickableSpan[] spans = optOutView.getClickableSpans();
        assertEquals(1, spans.length);
        spans[0].onClick(optOutView);

        assertTrue(mAdaptiveResponses.isEmpty());
        assertNotNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void testAdaptiveUi_onOptOut_afterInputThreshold_sendsOptOutOnce() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(/* showOptOut= */ true, /* informOnly= */ false);
        HistogramWatcher dismissWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.AdaptiveUi.DismissReason.ModalDialog",
                                DismissReason.DISMISSED_BY_FEATURE)
                        .build();
        assertTrue(mController.show());

        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        TextViewWithClickableSpans optOutView = mController.getViewForTesting().mOptOutText;
        ClickableSpan[] spans = optOutView.getClickableSpans();
        assertEquals(1, spans.length);
        spans[0].onClick(optOutView);

        assertEquals(List.of(SpcResponseStatus.OPT_OUT), mAdaptiveResponses);
        assertNull(mModalDialogManager.getShownDialogModel());
        dismissWatcher.assertExpected();
    }

    @Test
    public void testAdaptiveUi_queuedSheetShow_returnsFalseAndSendsNoResponse() {
        doReturn(false)
                .when(mBottomSheetController)
                .requestShowContent(any(BottomSheetContent.class), anyBoolean());
        attachAdaptiveUiController(/* isDesktop= */ false);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);
        HistogramWatcher deferredDismissWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.AdaptiveUi.DismissReasonBeforeShown.BottomSheet",
                                DismissReason.DISMISSED_BY_FEATURE)
                        .build();

        assertFalse(mController.show());
        assertTrue(mAdaptiveResponses.isEmpty());
        deferredDismissWatcher.assertExpected();
    }

    @Test
    public void testAdaptiveUi_queuedDialogShow_returnsFalseAndSendsNoResponse() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        mModalDialogManager.setQueueNextDialog(true);
        createAdaptiveController(/* showOptOut= */ false, /* informOnly= */ false);
        HistogramWatcher deferredDismissWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.AdaptiveUi.DismissReasonBeforeShown.ModalDialog",
                                DismissReason.DISMISSED_BY_FEATURE)
                        .build();

        assertFalse(mController.show());
        assertTrue(mAdaptiveResponses.isEmpty());
        assertNull(mModalDialogManager.getShownDialogModel());
        deferredDismissWatcher.assertExpected();
    }

    @Test
    public void testAdaptiveUi_transactionModeAutoAccept_acceptsOnce() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(
                /* showOptOut= */ false, /* informOnly= */ false, SPCTransactionMode.AUTO_ACCEPT);

        assertTrue(mController.show());
        assertEquals(List.of(SpcResponseStatus.ACCEPT), mAdaptiveResponses);
        assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void testAdaptiveUi_transactionModeAutoAcceptWithInformOnly_sendsAnotherWayOnce() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(
                /* showOptOut= */ false, /* informOnly= */ true, SPCTransactionMode.AUTO_ACCEPT);

        assertTrue(mController.show());
        assertEquals(List.of(SpcResponseStatus.ANOTHER_WAY), mAdaptiveResponses);
        assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void testAdaptiveUi_transactionModeAutoVerifyAnotherWay_sendsAnotherWayOnce() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(
                /* showOptOut= */ false,
                /* informOnly= */ false,
                SPCTransactionMode.AUTO_AUTH_ANOTHER_WAY);

        assertTrue(mController.show());
        assertEquals(List.of(SpcResponseStatus.ANOTHER_WAY), mAdaptiveResponses);
        assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void testAdaptiveUi_transactionModeAutoReject_cancelsOnce() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(
                /* showOptOut= */ false, /* informOnly= */ false, SPCTransactionMode.AUTO_REJECT);

        assertTrue(mController.show());
        assertEquals(List.of(SpcResponseStatus.CANCEL), mAdaptiveResponses);
        assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void testAdaptiveUi_transactionModeAutoOptOut_optsOutOnce() {
        attachAdaptiveUiController(/* isDesktop= */ true);
        createAdaptiveController(
                /* showOptOut= */ true, /* informOnly= */ false, SPCTransactionMode.AUTO_OPT_OUT);

        assertTrue(mController.show());
        assertEquals(List.of(SpcResponseStatus.OPT_OUT), mAdaptiveResponses);
        assertNull(mModalDialogManager.getShownDialogModel());
    }

    private void attachAdaptiveUiController(boolean isDesktop) {
        RuntimeEnvironment.getApplication().setTheme(R.style.Theme_BrowserUI_DayNight);
        DeviceInfo.setIsDesktopForTesting(isDesktop);
        mModalDialogManager = new TestModalDialogManager();
        mAdaptiveUiController =
                new AdaptiveUiControllerImpl(mWindow, mModalDialogManager, mBottomSheetController);
    }

    private BottomSheetContent showAdaptiveSheet() {
        assertTrue(mController.show());
        ArgumentCaptor<BottomSheetContent> contentCaptor =
                ArgumentCaptor.forClass(BottomSheetContent.class);
        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController)
                .requestShowContent(contentCaptor.capture(), eq(/* animate= */ true));
        verify(mBottomSheetController).addObserver(observerCaptor.capture());

        BottomSheetContent sheetContent = contentCaptor.getValue();
        BottomSheetObserver sheetObserver = observerCaptor.getValue();
        doReturn(sheetContent).when(mBottomSheetController).getCurrentSheetContent();
        sheetObserver.onSheetContentChanged(sheetContent);
        sheetObserver.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        return sheetContent;
    }

    private void createAdaptiveController(boolean showOptOut, boolean informOnly) {
        createAdaptiveController(showOptOut, informOnly, SPCTransactionMode.NONE);
    }

    private void createAdaptiveController(
            boolean showOptOut, boolean informOnly, @SPCTransactionMode int transactionMode) {
        // ChromePaymentRequestService closes SPC on every response except ACCEPT.
        Callback<Integer> reentrantResponseCallback =
                status -> {
                    mAdaptiveResponses.add(status);
                    if (status != SpcResponseStatus.ACCEPT) {
                        mController.hide();
                    }
                };
        mController =
                new SecurePaymentConfirmationController(
                        mWindow,
                        mPaymentEntityLogos,
                        mPayeeName,
                        mPayeeOrigin,
                        mPaymentInstrumentLabelPrimary,
                        mPaymentInstrumentLabelSecondary,
                        mTotal,
                        mPaymentIcon,
                        mRelyingPartyId,
                        showOptOut,
                        informOnly,
                        reentrantResponseCallback,
                        transactionMode);
        mController.setInputProtectorForTesting(new InputProtector(mClock));
    }

    private void createController(boolean showOptOut, boolean informOnly) {
        createController(showOptOut, informOnly, SPCTransactionMode.NONE);
    }

    private void createController(
            boolean showOptOut, boolean informOnly, @SPCTransactionMode int transactionMode) {
        mController =
                new SecurePaymentConfirmationController(
                        mWindow,
                        mPaymentEntityLogos,
                        mPayeeName,
                        mPayeeOrigin,
                        mPaymentInstrumentLabelPrimary,
                        mPaymentInstrumentLabelSecondary,
                        mTotal,
                        mPaymentIcon,
                        mRelyingPartyId,
                        showOptOut,
                        informOnly,
                        mResponseCallback,
                        transactionMode);
        InputProtector inputProtector = new InputProtector(mClock);
        inputProtector.markShowTime();
        mController.setInputProtectorForTesting(inputProtector);
        ((BottomSheetPresenter) mController.getPresenterForTesting())
                .setObserverForTesting(mBottomSheetObserver);
    }

    private AdaptiveUiEntry adaptiveEntry() {
        assertTrue(mController.getPresenterForTesting() instanceof AdaptiveUiPresenter);
        return ((AdaptiveUiPresenter) mController.getPresenterForTesting()).getEntryForTesting();
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
