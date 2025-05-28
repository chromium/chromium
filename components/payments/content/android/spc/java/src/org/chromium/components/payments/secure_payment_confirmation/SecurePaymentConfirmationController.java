// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.util.Pair;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.payments.R;
import org.chromium.components.payments.ui.CurrencyFormatter;
import org.chromium.components.payments.ui.InputProtector;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.url.Origin;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/**
 * The controller of the SecurePaymentConfirmation UI, which owns the component overall, i.e.,
 * creates other objects in the component and connects them. It decouples the implementation of this
 * component from other components and acts as the point of contact between them. Any code in this
 * component that needs to interact with another component does that through this controller.
 */
@NullMarked
public class SecurePaymentConfirmationController
        implements SecurePaymentConfirmationBottomSheetObserver.ControllerDelegate {
    @IntDef({
        SpcResponseStatus.UNKNOWN,
        SpcResponseStatus.ACCEPT,
        SpcResponseStatus.ANOTHER_WAY,
        SpcResponseStatus.CANCEL,
        SpcResponseStatus.OPT_OUT,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SpcResponseStatus {
        int UNKNOWN = 0;
        int ACCEPT = 1;
        int ANOTHER_WAY = 2;
        int CANCEL = 3;
        int OPT_OUT = 4;
        int COUNT = 5;
    }

    private final SecurePaymentConfirmationBottomSheetContent mContent;
    private final SecurePaymentConfirmationView mView;
    private final PropertyModel mModel;
    private final BottomSheetController mBottomSheetController;
    private final Callback<Integer> mResponseCallback;
    private final Boolean mInformOnly;
    private SecurePaymentConfirmationBottomSheetObserver mBottomSheetObserver;
    private InputProtector mInputProtector = new InputProtector();

    /**
     * Constructs the SPC UI component controller.
     *
     * @param window The WindowAndroid of the merchant.
     * @param payeeName The name of the payee, or null if not specified.
     * @param payeeOrigin The origin of the payee, or null if not specified.
     * @param paymentInstrumentLabel The label to display for the payment instrument.
     * @param total The total amount of the transaction.
     * @param paymentIcon The icon of the payment instrument.
     * @param issuerIcon The icon of the issuer.
     * @param networkIcon The icon of the network.
     * @param relyingPartyId The relying party ID for the SPC credential.
     * @param showOptOut Whether to show the opt out UX to the user.
     * @param informOnly Whether to show the inform-only UX.
     * @param responseCallback The function to call on sheet dismiss; called with SpcResponseStatus.
     */
    public SecurePaymentConfirmationController(
            WindowAndroid window,
            @Nullable String payeeName,
            @Nullable Origin payeeOrigin,
            String paymentInstrumentLabel,
            PaymentItem total,
            Drawable paymentIcon,
            @Nullable Drawable issuerIcon,
            @Nullable Drawable networkIcon,
            String relyingPartyId,
            boolean showOptOut,
            boolean informOnly,
            Callback<Integer> responseCallback) {
        Context context = window.getContext().get();
        assertNonNull(context);
        BottomSheetController bottomSheetController = BottomSheetControllerProvider.from(window);
        assertNonNull(bottomSheetController);

        mBottomSheetController = bottomSheetController;
        mInformOnly = informOnly;
        mResponseCallback = responseCallback;
        mInputProtector.markShowTime();

        // The instrument icon may be empty, if it couldn't be downloaded/decoded and
        // iconMustBeShown was set to false. In that case, use a default icon. The actual display
        // color is set based on the theme in OnThemeChanged.
        boolean usingDefaultIcon = false;
        assert paymentIcon instanceof BitmapDrawable;
        if (((BitmapDrawable) paymentIcon).getBitmap() == null) {
            paymentIcon =
                    ResourcesCompat.getDrawable(
                            context.getResources(), R.drawable.credit_card, context.getTheme());
            usingDefaultIcon = true;
        }

        SpannableString optOutText = null;
        if (showOptOut) {
            // Attempt to determine whether the current device is a tablet or not. This method is
            // quite inaccurate, but is only used for customizing the opt out UX and so getting it
            // wrong is low-cost.
            String deviceString =
                    context.getString(
                            DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                                    ? R.string.secure_payment_confirmation_this_tablet_label
                                    : R.string.secure_payment_confirmation_this_phone_label);
            optOutText =
                    SpanApplier.applySpans(
                            context.getString(
                                    R.string.secure_payment_confirmation_opt_out_label,
                                    deviceString,
                                    relyingPartyId),
                            new SpanInfo(
                                    "BEGIN_LINK",
                                    "END_LINK",
                                    new ChromeClickableSpan(context, (widget) -> onOptOut())));
        }

        boolean showsIssuerNetworkIcons = issuerIcon != null && networkIcon != null;

        SpannableString footnote = null;
        if (!mInformOnly) {
            footnote =
                    SpanApplier.applySpans(
                            context.getString(R.string.secure_payment_confirmation_footnote),
                            new SpanInfo(
                                    "BEGIN_LINK",
                                    "END_LINK",
                                    new ChromeClickableSpan(
                                            context, (widget) -> onVerifyAnotherWay())));
        }

        mView = new SecurePaymentConfirmationView(context);
        mModel =
                new PropertyModel.Builder(SecurePaymentConfirmationProperties.ALL_KEYS)
                        .with(
                                SecurePaymentConfirmationProperties.SHOWS_ISSUER_NETWORK_ICONS,
                                showsIssuerNetworkIcons)
                        .with(SecurePaymentConfirmationProperties.ISSUER_ICON, issuerIcon)
                        .with(SecurePaymentConfirmationProperties.NETWORK_ICON, networkIcon)
                        .with(
                                SecurePaymentConfirmationProperties.TITLE,
                                mInformOnly
                                        ? context.getString(
                                                R.string
                                                        .secure_payment_confirmation_inform_only_title)
                                        : context.getString(
                                                R.string
                                                        .secure_payment_confirmation_verify_purchase))
                        .with(
                                SecurePaymentConfirmationProperties.STORE_LABEL,
                                getStoreLabel(payeeName, payeeOrigin))
                        .with(
                                SecurePaymentConfirmationProperties.PAYMENT_ICON,
                                Pair.create(paymentIcon, usingDefaultIcon))
                        .with(
                                SecurePaymentConfirmationProperties.PAYMENT_INSTRUMENT_LABEL,
                                paymentInstrumentLabel)
                        .with(SecurePaymentConfirmationProperties.CURRENCY, total.amount.currency)
                        .with(SecurePaymentConfirmationProperties.TOTAL, formatPaymentItem(total))
                        .with(SecurePaymentConfirmationProperties.OPT_OUT_TEXT, optOutText)
                        .with(SecurePaymentConfirmationProperties.FOOTNOTE, footnote)
                        .with(
                                SecurePaymentConfirmationProperties.CONTINUE_BUTTON_LABEL,
                                mInformOnly
                                        ? context.getString(R.string.payments_confirm_button)
                                        : context.getString(R.string.payments_continue_button))
                        .build();
        PropertyModelChangeProcessor.create(
                mModel, mView, SecurePaymentConfirmationViewBinder::bind);
        mView.mContinueButton.setOnClickListener(
                (View button) -> {
                    onContinue();
                });
        mView.mCancelButton.setOnClickListener(
                (View button) -> {
                    onCancel();
                });

        mContent =
                new SecurePaymentConfirmationBottomSheetContent(
                        mView.mContentView, mView.mScrollView);
        mBottomSheetObserver =
                new SecurePaymentConfirmationBottomSheetObserver(mBottomSheetController);
    }

    /** Requests to show the SPC UI. Returns true if shown and false otherwise. */
    public boolean show() {
        if (mBottomSheetController.requestShowContent(mContent, /* animate= */ true)) {
            mBottomSheetObserver.begin(this);
            return true;
        }
        return false;
    }

    /** Hides the SPC UI (if showing). */
    public void hide() {
        mBottomSheetObserver.end();
        mBottomSheetController.hideContent(mContent, /* animate= */ true);
    }

    private void onContinue() {
        if (!mInputProtector.shouldInputBeProcessed()) return;
        hide();
        if (mInformOnly) {
            mResponseCallback.onResult(SpcResponseStatus.ANOTHER_WAY);
        } else {
            mResponseCallback.onResult(SpcResponseStatus.ACCEPT);
        }
    }

    @Override
    public void onCancel() {
        if (!mInputProtector.shouldInputBeProcessed()) return;
        hide();
        mResponseCallback.onResult(SpcResponseStatus.CANCEL);
    }

    private void onVerifyAnotherWay() {
        if (!mInputProtector.shouldInputBeProcessed()) return;
        hide();
        mResponseCallback.onResult(SpcResponseStatus.ANOTHER_WAY);
    }

    private void onOptOut() {
        if (!mInputProtector.shouldInputBeProcessed()) return;
        hide();
        mResponseCallback.onResult(SpcResponseStatus.OPT_OUT);
    }

    private String getStoreLabel(@Nullable String payeeName, @Nullable Origin payeeOrigin) {
        // At least one of the payeeName and payeeOrigin must be non-null in SPC; this should be
        // enforced by PaymentRequestService.isValidSecurePaymentConfirmationRequest.
        assert payeeName != null || payeeOrigin != null;

        if (payeeOrigin == null) return assertNonNull(payeeName);

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

    /*package*/ SecurePaymentConfirmationView getViewForTesting() {
        return mView;
    }

    /*package*/ PropertyModel getModelForTesting() {
        return mModel;
    }

    /*package*/ void setInputProtectorForTesting(InputProtector inputProtector) {
        mInputProtector = inputProtector;
    }

    /*package*/ void setBottomSheetObserverForTesting(
            SecurePaymentConfirmationBottomSheetObserver observer) {
        mBottomSheetObserver = observer;
    }
}
