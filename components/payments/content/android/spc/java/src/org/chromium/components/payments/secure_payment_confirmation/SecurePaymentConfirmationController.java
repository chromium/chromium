// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.payments.PaymentApp.PaymentEntityLogo;
import org.chromium.components.payments.R;
import org.chromium.components.payments.SPCTransactionMode;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationBottomSheetObserver.ControllerDelegate;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationProperties.ItemProperties;
import org.chromium.components.payments.ui.CurrencyFormatter;
import org.chromium.components.payments.ui.InputProtector;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.url.Origin;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Locale;
import java.util.Objects;

/**
 * The controller of the SecurePaymentConfirmation UI, which owns the component overall, i.e.,
 * creates other objects in the component and connects them. It decouples the implementation of this
 * component from other components and acts as the point of contact between them. Any code in this
 * component that needs to interact with another component does that through this controller.
 */
@NullMarked
public class SecurePaymentConfirmationController implements ControllerDelegate {
    /** There is only a single model/view for SPC items so only a single item type is needed. */
    private static final int SPC_ITEM_TYPE = 0;

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
    private final Boolean mShowOptOut;
    private final Boolean mInformOnly;
    private final Callback<Integer> mResponseCallback;
    private final @SPCTransactionMode int mTransactionMode;
    private SecurePaymentConfirmationBottomSheetObserver mBottomSheetObserver;
    private InputProtector mInputProtector = new InputProtector();

    /**
     * Constructs the SPC UI component controller.
     *
     * @param window The WindowAndroid of the merchant.
     * @param payeeName The name of the payee, or null if not specified.
     * @param payeeOrigin The origin of the payee, or null if not specified.
     * @param paymentInstrumentLabelPrimary The label to display for the payment instrument.
     * @param paymentItem The payment item of the transaction containing total payment information.
     * @param paymentIcon The icon of the payment instrument.
     * @param issuerIcon The icon of the issuer.
     * @param networkIcon The icon of the network.
     * @param relyingPartyId The relying party ID for the SPC credential.
     * @param showOptOut Whether to show the opt out UX to the user.
     * @param informOnly Whether to show the inform-only UX.
     * @param responseCallback The function to call on sheet dismiss; called with SpcResponseStatus.
     * @param transactionMode The automation transaction mode; NONE when not under automation.
     */
    public SecurePaymentConfirmationController(
            WindowAndroid window,
            List<PaymentEntityLogo> paymentEntityLogos,
            @Nullable String payeeName,
            @Nullable Origin payeeOrigin,
            String paymentInstrumentLabelPrimary,
            @Nullable String paymentInstrumentLabelSecondary,
            PaymentItem paymentItem,
            Drawable paymentIcon,
            String relyingPartyId,
            boolean showOptOut,
            boolean informOnly,
            Callback<Integer> responseCallback,
            @SPCTransactionMode int transactionMode) {
        Context context = assertNonNull(window.getContext().get());
        BottomSheetController bottomSheetController =
                assertNonNull(BottomSheetControllerProvider.from(window));

        mBottomSheetController = bottomSheetController;
        mShowOptOut = showOptOut;
        mInformOnly = informOnly;
        mResponseCallback = responseCallback;
        mTransactionMode = transactionMode;
        mInputProtector.markShowTime();

        ModelList itemList = new ModelList();
        // Set the store primary and secondary text accordingly (whether only one or both are
        // populated). Note that at least one of these must be non-null in SPC; this should be
        // enforced by PaymentRequestService.isValidSecurePaymentConfirmationRequest().
        assertNonNull(payeeName != null ? payeeName : payeeOrigin);
        String storePrimaryText;
        String storeSecondaryText = null;
        if (payeeName == null || payeeName.isEmpty()) {
            storePrimaryText =
                    UrlFormatter.formatOriginForSecurityDisplay(
                            Objects.requireNonNull(payeeOrigin), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        } else {
            storePrimaryText = payeeName;
            if (payeeOrigin != null) {
                storeSecondaryText =
                        UrlFormatter.formatOriginForSecurityDisplay(
                                payeeOrigin, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
            }
        }
        // Add the store row.
        itemList.add(
                new ListItem(
                        SPC_ITEM_TYPE,
                        new PropertyModel.Builder(ItemProperties.ALL_KEYS)
                                .with(
                                        ItemProperties.ICON,
                                        ResourcesCompat.getDrawable(
                                                context.getResources(),
                                                R.drawable.storefront_icon,
                                                context.getTheme()))
                                .with(
                                        ItemProperties.ICON_LABEL,
                                        context.getString(
                                                R.string.secure_payment_confirmation_store_label))
                                .with(ItemProperties.PRIMARY_TEXT, storePrimaryText)
                                .with(ItemProperties.SECONDARY_TEXT, storeSecondaryText)
                                .build()));

        // The instrument icon may be empty, if it couldn't be downloaded/decoded and
        // iconMustBeShown was set to false. In that case, use a default icon. The actual display
        // color is set based on the theme in OnThemeChanged.
        assert paymentIcon instanceof BitmapDrawable;
        if (((BitmapDrawable) paymentIcon).getBitmap() == null) {
            paymentIcon =
                    ResourcesCompat.getDrawable(
                            context.getResources(), R.drawable.credit_card, context.getTheme());
        }
        // Add the payment row.
        itemList.add(
                new ListItem(
                        SPC_ITEM_TYPE,
                        new PropertyModel.Builder(ItemProperties.ALL_KEYS)
                                .with(ItemProperties.ICON, paymentIcon)
                                .with(ItemProperties.PRIMARY_TEXT, paymentInstrumentLabelPrimary)
                                .with(
                                        ItemProperties.SECONDARY_TEXT,
                                        paymentInstrumentLabelSecondary)
                                .build()));

        // Convert the total value to the local currency amount.
        CurrencyFormatter currencyFormatter =
                new CurrencyFormatter(paymentItem.amount.currency, Locale.getDefault());
        String totalValue = currencyFormatter.format(paymentItem.amount.value);
        currencyFormatter.destroy();
        // Add the total row.
        itemList.add(
                new ListItem(
                        SPC_ITEM_TYPE,
                        new PropertyModel.Builder(ItemProperties.ALL_KEYS)
                                .with(
                                        ItemProperties.ICON,
                                        ResourcesCompat.getDrawable(
                                                context.getResources(),
                                                R.drawable.payments_icon,
                                                context.getTheme()))
                                .with(
                                        ItemProperties.ICON_LABEL,
                                        context.getString(
                                                R.string.secure_payment_confirmation_total_label))
                                .with(
                                        ItemProperties.PRIMARY_TEXT,
                                        String.format(
                                                "%s %s", paymentItem.amount.currency, totalValue))
                                .build()));
        SimpleRecyclerViewAdapter itemListAdapter = new SimpleRecyclerViewAdapter(itemList);
        itemListAdapter.registerType(
                SPC_ITEM_TYPE,
                SecurePaymentConfirmationView::createItemView,
                SecurePaymentConfirmationViewBinder::bindItem);

        SpannableString optOutText = null;
        if (mShowOptOut) {
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
                        .with(SecurePaymentConfirmationProperties.HEADER_LOGOS, paymentEntityLogos)
                        .with(
                                SecurePaymentConfirmationProperties.TITLE,
                                mInformOnly
                                        ? context.getString(
                                                R.string
                                                        .secure_payment_confirmation_inform_only_title)
                                        : context.getString(
                                                R.string.secure_payment_confirmation_title))
                        .with(
                                SecurePaymentConfirmationProperties.ITEM_LIST_ADAPTER,
                                itemListAdapter)
                        .with(SecurePaymentConfirmationProperties.OPT_OUT_TEXT, optOutText)
                        .with(SecurePaymentConfirmationProperties.FOOTNOTE, footnote)
                        .with(
                                SecurePaymentConfirmationProperties.CONTINUE_BUTTON_LABEL,
                                mInformOnly
                                        ? context.getString(R.string.payments_confirm_button)
                                        : context.getString(
                                                R.string
                                                        .secure_payment_confirmation_verify_button_label))
                        .build();
        PropertyModelChangeProcessor.create(
                mModel, mView, SecurePaymentConfirmationViewBinder::bind);
        mView.mContinueButton.setOnClickListener(
                (View button) -> {
                    onContinue();
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

            // For browser automation use-cases (such as WebDriver), SPC can be placed in
            // transaction mode where it will immediately take some action on the dialog without
            // user interaction.  We deliberately wait until after the dialog is created and shown
            // to handle this, in order to keep the automation codepath close to the real one.
            //
            // https://w3c.github.io/secure-payment-confirmation/#sctn-transaction-ux-test-automation
            switch (mTransactionMode) {
                case SPCTransactionMode.AUTO_ACCEPT:
                    onContinue();
                    break;
                case SPCTransactionMode.AUTO_AUTH_ANOTHER_WAY:
                    // To best mimic the underlying dialog, in mInformOnly mode we still click on
                    // the 'Continue' button.
                    if (mInformOnly) {
                        onContinue();
                    } else {
                        onVerifyAnotherWay();
                    }
                    break;
                case SPCTransactionMode.AUTO_OPT_OUT:
                    onOptOut();
                    break;
                case SPCTransactionMode.AUTO_REJECT:
                    onCancel();
                    break;
            }

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
        if (!mInputProtector.shouldInputBeProcessed()
                && mTransactionMode == SPCTransactionMode.NONE) {
            return;
        }

        hide();
        if (mInformOnly) {
            mResponseCallback.onResult(SpcResponseStatus.ANOTHER_WAY);
        } else {
            mResponseCallback.onResult(SpcResponseStatus.ACCEPT);
        }
    }

    @Override
    public void onCancel() {
        if (!mInputProtector.shouldInputBeProcessed()
                && mTransactionMode == SPCTransactionMode.NONE) {
            return;
        }
        hide();
        mResponseCallback.onResult(SpcResponseStatus.CANCEL);
    }

    private void onVerifyAnotherWay() {
        if (!mInputProtector.shouldInputBeProcessed()
                && mTransactionMode == SPCTransactionMode.NONE) {
            return;
        }
        hide();
        mResponseCallback.onResult(SpcResponseStatus.ANOTHER_WAY);
    }

    private void onOptOut() {
        if (!mInputProtector.shouldInputBeProcessed()
                && mTransactionMode == SPCTransactionMode.NONE) {
            return;
        }
        hide();
        mResponseCallback.onResult(SpcResponseStatus.OPT_OUT);
    }

    /**
     * Called by PaymentRequestTestBridge for cross-platform browser tests, the following methods
     * bypass the input protector. The Java unit tests simulate clicking the button and therefore
     * test the input protector.
     */
    public boolean cancelForTest() {
        hide();
        mResponseCallback.onResult(SpcResponseStatus.CANCEL);
        return true;
    }

    public boolean optOutForTest() {
        if (!mShowOptOut) {
            return false;
        }
        hide();
        mResponseCallback.onResult(SpcResponseStatus.OPT_OUT);
        return true;
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
