// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.payments.InputProtector;
import org.chromium.components.payments.R;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.Origin;

import java.util.Locale;

/**
 * The controller of the SecurePaymentConfirmation Authn UI, which owns the component overall, i.e.,
 * creates other objects in the component and connects them. It decouples the implementation of this
 * component from other components and acts as the point of contact between them. Any code in this
 * component that needs to interact with another component does that through this controller.
 */
public class SecurePaymentConfirmationAuthnController {
    private final WebContents mWebContents;
    private Runnable mHider;
    private Callback<Boolean> mResponseCallback;
    private Runnable mOptOutCallback;
    private SecurePaymentConfirmationAuthnView mView;

    private InputProtector mInputProtector = new InputProtector();

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetStateChanged(int newState, int reason) {
                    switch (newState) {
                        case BottomSheetController.SheetState.HIDDEN:
                            onCancel();
                            break;
                    }
                }
            };

    private final BottomSheetContent mBottomSheetContent =
            new BottomSheetContent() {
                @Override
                public View getContentView() {
                    return mView.getContentView();
                }

                @Override
                public View getToolbarView() {
                    return null;
                }

                @Override
                public int getVerticalScrollOffset() {
                    if (mView != null) {
                        return mView.getScrollY();
                    }

                    return 0;
                }

                @Override
                public float getFullHeightRatio() {
                    return HeightMode.WRAP_CONTENT;
                }

                @Override
                public float getHalfHeightRatio() {
                    return HeightMode.DISABLED;
                }

                @Override
                public void destroy() {}

                @Override
                public int getPriority() {
                    return ContentPriority.HIGH;
                }

                @Override
                public int getPeekHeight() {
                    return HeightMode.DISABLED;
                }

                @Override
                public boolean swipeToDismissEnabled() {
                    return false;
                }

                @Override
                public int getSheetContentDescriptionStringId() {
                    return R.string.secure_payment_confirmation_authentication_sheet_description;
                }

                @Override
                public int getSheetHalfHeightAccessibilityStringId() {
                    assert false : "This method should not be called";
                    return 0;
                }

                @Override
                public int getSheetFullHeightAccessibilityStringId() {
                    return R.string.secure_payment_confirmation_authentication_sheet_opened;
                }

                @Override
                public int getSheetClosedAccessibilityStringId() {
                    return R.string.secure_payment_confirmation_authentication_sheet_closed;
                }
            };

    /**
     * Constructs the SPC Authn UI component controller.
     *
     * @param webContents The WebContents of the merchant.
     */
    public static SecurePaymentConfirmationAuthnController create(WebContents webContents) {
        return webContents != null
                ? new SecurePaymentConfirmationAuthnController(webContents)
                : null;
    }

    private SecurePaymentConfirmationAuthnController(WebContents webContents) {
        mWebContents = webContents;
    }

    /**
     * Shows the SPC Authn UI.
     *
     * @param paymentIcon The icon of the payment instrument.
     * @param paymentInstrumentLabel The label to display for the payment instrument.
     * @param total The total amount of the transaction.
     * @param responseCallback The function to call on sheet dismiss; false if it failed.
     * @param optOutCallback The function to call on user opt out.
     * @param payeeName The name of the payee, or null if not specified.
     * @param payeeOrigin The origin of the payee, or null if not specified.
     * @param showOptOut Whether to show the opt out UX to the user.
     * @param rpId The relying party ID for the SPC credential.
     */
    public boolean show(
            Drawable paymentIcon,
            String paymentInstrumentLabel,
            PaymentItem total,
            Callback<Boolean> responseCallback,
            Runnable optOutCallback,
            @Nullable String payeeName,
            @Nullable Origin payeeOrigin,
            boolean showOptOut,
            String rpId) {
        if (mHider != null) return false;

        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return false;
        Context context = windowAndroid.getContext().get();
        if (context == null) return false;

        BottomSheetController bottomSheet = BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheet == null) return false;

        mInputProtector.markShowTime();

        // The instrument icon may be empty, if it couldn't be downloaded/decoded
        // and iconMustBeShown was set to false. In that case, use a default icon.
        // The actual display color is set based on the theme in OnThemeChanged.
        boolean usingDefaultIcon = false;
        assert paymentIcon instanceof BitmapDrawable;
        if (((BitmapDrawable) paymentIcon).getBitmap() == null) {
            paymentIcon =
                    ResourcesCompat.getDrawable(
                            context.getResources(), R.drawable.credit_card, context.getTheme());
            usingDefaultIcon = true;
        }

        SecurePaymentConfirmationAuthnView.OptOutInfo optOutInfo =
                new SecurePaymentConfirmationAuthnView.OptOutInfo(
                        showOptOut, rpId, this::onOptOutPressed);

        PropertyModel model =
                new PropertyModel.Builder(SecurePaymentConfirmationAuthnProperties.ALL_KEYS)
                        .with(
                                SecurePaymentConfirmationAuthnProperties.STORE_LABEL,
                                getStoreLabel(payeeName, payeeOrigin))
                        .with(
                                SecurePaymentConfirmationAuthnProperties.PAYMENT_ICON,
                                Pair.create(paymentIcon, usingDefaultIcon))
                        .with(
                                SecurePaymentConfirmationAuthnProperties.PAYMENT_INSTRUMENT_LABEL,
                                paymentInstrumentLabel)
                        .with(
                                SecurePaymentConfirmationAuthnProperties.TOTAL,
                                formatPaymentItem(total))
                        .with(
                                SecurePaymentConfirmationAuthnProperties.CURRENCY,
                                total.amount.currency)
                        .with(SecurePaymentConfirmationAuthnProperties.OPT_OUT_INFO, optOutInfo)
                        .with(
                                SecurePaymentConfirmationAuthnProperties.CONTINUE_BUTTON_CALLBACK,
                                this::onConfirmPressed)
                        .with(
                                SecurePaymentConfirmationAuthnProperties.CANCEL_BUTTON_CALLBACK,
                                this::onCancelPressed)
                        .build();

        bottomSheet.addObserver(mBottomSheetObserver);

        mView = new SecurePaymentConfirmationAuthnView(context);
        PropertyModelChangeProcessor changeProcessor =
                PropertyModelChangeProcessor.create(
                        model, mView, SecurePaymentConfirmationAuthnViewBinder::bind);

        mHider =
                () -> {
                    changeProcessor.destroy();
                    bottomSheet.removeObserver(mBottomSheetObserver);
                    bottomSheet.hideContent(
                            /* content= */ mBottomSheetContent, /* animate= */ true);
                };

        mResponseCallback = responseCallback;
        mOptOutCallback = showOptOut ? optOutCallback : null;

        boolean isShowSuccess =
                bottomSheet.requestShowContent(mBottomSheetContent, /* animate= */ true);

        if (!isShowSuccess) {
            hide();
            return false;
        }

        return true;
    }

    /** Hides the SPC Authn UI. */
    public void hide() {
        if (mHider == null) return;
        mHider.run();
        mHider = null;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public SecurePaymentConfirmationAuthnView getView() {
        return mView;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public boolean isHidden() {
        return mHider == null;
    }

    private String getStoreLabel(@Nullable String payeeName, @Nullable Origin payeeOrigin) {
        // At least one of the payeeName and payeeOrigin must be non-null in SPC; this should be
        // enforced by PaymentRequestService.isValidSecurePaymentConfirmationRequest.
        assert payeeName != null || payeeOrigin != null;

        if (payeeOrigin == null) return payeeName;

        String origin =
                UrlFormatter.formatOriginForSecurityDisplay(
                        payeeOrigin, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        return payeeName == null ? origin : String.format("%s (%s)", payeeName, origin);
    }

    private String formatPaymentItem(PaymentItem paymentItem) {
        CurrencyFormatter formatter =
                new CurrencyFormatter(paymentItem.amount.currency, Locale.getDefault());
        String result = formatter.format(paymentItem.amount.value);
        formatter.destroy();
        return result;
    }

    private void onConfirm() {
        hide();
        mResponseCallback.onResult(true);
    }

    private void onConfirmPressed() {
        if (mInputProtector.shouldInputBeProcessed()) onConfirm();
    }

    private void onCancel() {
        hide();
        mResponseCallback.onResult(false);
    }

    private void onCancelPressed() {
        if (mInputProtector.shouldInputBeProcessed()) onCancel();
    }

    private void onOptOut() {
        assert mOptOutCallback != null;
        hide();
        mOptOutCallback.run();
    }

    private void onOptOutPressed() {
        if (mInputProtector.shouldInputBeProcessed()) onOptOut();
    }

    void setInputProtectorForTesting(InputProtector inputProtector) {
        mInputProtector = inputProtector;
    }

    /**
     * Called by PaymentRequestTestBridge for cross-platform browsertests, the following methods
     * bypass the input protector. The Java unit tests simulate clicking the button and therefore
     * test the input protector.
     */
    public boolean cancelForTest() {
        onCancel();
        return true;
    }

    public boolean optOutForTest() {
        if (mOptOutCallback == null) return false;
        onOptOut();
        return true;
    }
}
