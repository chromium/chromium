// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiContainer;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiController;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiEntry;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiScope;
import org.chromium.components.browser_ui.adaptive_ui.DismissReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.UserCriticalFeature;
import org.chromium.components.payments.PaymentApp.PaymentEntityLogo;
import org.chromium.components.payments.SPCTransactionMode;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationBottomSheetObserver.ControllerDelegate;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationProperties.ItemProperties;
import org.chromium.components.payments.ui.CurrencyFormatter;
import org.chromium.components.payments.ui.InputProtector;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
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

    // LINT.IfChange(SecurePaymentRequestOutcome)
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

    // LINT.ThenChange(//tools/metrics/histograms/metadata/payment/enums.xml:SecurePaymentRequestOutcome)

    @IntDef({SpcState.NOT_SHOWN, SpcState.SHOWING, SpcState.CLOSING, SpcState.CLOSED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface SpcState {
        /** show() hasn't succeeded. */
        int NOT_SHOWN = 0;

        /** Showing, and no response sent yet. */
        int SHOWING = 1;

        /** The Adaptive UI primary button responded, and the framework is closing the UI. */
        int CLOSING = 2;

        /** A response was sent, or hide() ran. */
        int CLOSED = 3;
    }

    /** Shows and hides the SPC UI, in the legacy bottom sheet or through Adaptive UI. */
    /*package*/ interface Presenter {
        /** Returns false if the UI couldn't be shown. */
        boolean show();

        /** Hides the UI. Sends no response. */
        void hide();
    }

    /*package*/ final class BottomSheetPresenter implements Presenter {
        private final BottomSheetController mBottomSheetController;
        private final SecurePaymentConfirmationBottomSheetContent mContent;
        private SecurePaymentConfirmationBottomSheetObserver mObserver;

        BottomSheetPresenter(BottomSheetController bottomSheetController) {
            mBottomSheetController = bottomSheetController;
            mInputProtector.markShowTime();
            mView.mContinueButton.setOnClickListener(
                    (View button) -> {
                        onContinue();
                    });
            mContent =
                    new SecurePaymentConfirmationBottomSheetContent(
                            mView.mContentView, mView.mScrollView);
            mObserver = new SecurePaymentConfirmationBottomSheetObserver(bottomSheetController);
        }

        @Override
        public boolean show() {
            if (!mBottomSheetController.requestShowContent(mContent, /* animate= */ true)) {
                return false;
            }
            mObserver.begin(SecurePaymentConfirmationController.this);
            return true;
        }

        @Override
        public void hide() {
            mObserver.end();
            mBottomSheetController.hideContent(mContent, /* animate= */ true);
        }

        /*package*/ void setObserverForTesting(
                SecurePaymentConfirmationBottomSheetObserver observer) {
            mObserver = observer;
        }
    }

    /*package*/ final class AdaptiveUiPresenter
            implements Presenter, AdaptiveUiProperties.Delegate {
        private final AdaptiveUiController mAdaptiveUiController;
        private final AdaptiveUiEntry mEntry;

        AdaptiveUiPresenter(
                Context context,
                AdaptiveUiController adaptiveUiController,
                String primaryButtonText) {
            mAdaptiveUiController = adaptiveUiController;
            // Both Adaptive UI containers already host CUSTOM_VIEW inside a scroll container, so
            // detach the inner body layout from SPC's own ScrollView and move the primary action
            // onto AdaptiveUiProperties.
            View customView = assertNonNull(mView.mContentView.getChildAt(0));
            mView.mContentView.removeView(customView);
            mView.mContinueButton.setVisibility(View.GONE);
            // Clear the bottom padding that sat below SPC's hidden Continue button; the sheet and
            // dialog add their own padding below the buttons.
            customView.setPaddingRelative(
                    customView.getPaddingStart(),
                    customView.getPaddingTop(),
                    customView.getPaddingEnd(),
                    0);
            PropertyModel adaptiveModel =
                    new PropertyModel.Builder(AdaptiveUiProperties.ALL_KEYS)
                            .with(AdaptiveUiProperties.DELEGATE, this)
                            .with(AdaptiveUiProperties.CUSTOM_VIEW, customView)
                            .with(
                                    AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION,
                                    context.getString(
                                            R.string
                                                    .secure_payment_confirmation_authentication_sheet_description))
                            .with(
                                    AdaptiveUiProperties.SHEET_OPENED_ACCESSIBILITY_STRING_ID,
                                    R.string
                                            .secure_payment_confirmation_authentication_sheet_opened)
                            .with(
                                    AdaptiveUiProperties.SHEET_CLOSED_ACCESSIBILITY_STRING_ID,
                                    R.string
                                            .secure_payment_confirmation_authentication_sheet_closed)
                            .with(
                                    AdaptiveUiProperties.SHEET_USER_CRITICAL,
                                    UserCriticalFeature.SECURE_PAYMENT_CONFIRMATION)
                            .with(AdaptiveUiProperties.SHOW_SHEET_HANDLEBAR, true)
                            .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, primaryButtonText)
                            .with(
                                    AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS,
                                    InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD)
                            .build();
            mEntry =
                    new AdaptiveUiEntry.Builder(adaptiveModel)
                            .setScope(AdaptiveUiScope.TRANSIENT)
                            .addAllowedContainer(AdaptiveUiContainer.BOTTOM_SHEET)
                            .addAllowedContainer(AdaptiveUiContainer.MODAL_DIALOG)
                            .build();
        }

        @Override
        public boolean show() {
            if (mAdaptiveUiController.show(mEntry) == null) {
                // SPC fails instead of waiting in a queue, so cancel any queued entry.
                mAdaptiveUiController.dismiss(mEntry, DismissReason.DISMISSED_BY_FEATURE);
                return false;
            }
            return true;
        }

        @Override
        public void hide() {
            mAdaptiveUiController.dismiss(mEntry, DismissReason.DISMISSED_BY_FEATURE);
        }

        @Override
        public void onShown(@AdaptiveUiContainer int container) {
            // Start the input protector when the sheet or dialog is shown, like the primary
            // button's tap protection.
            mInputProtector.markShowTime();
        }

        @Override
        public void onPrimaryButtonClicked() {
            if (mState != SpcState.SHOWING) return;
            mState = SpcState.CLOSING;
            // Respond directly instead of through onContinue(). Adaptive UI already applied tap
            // protection and will close the UI, so mInputProtector must not drop this tap.
            mResponseCallback.onResult(
                    mInformOnly ? SpcResponseStatus.ANOTHER_WAY : SpcResponseStatus.ACCEPT);
        }

        @Override
        public void onDismissed(@DismissReason int reason) {
            boolean wasShowing = mState == SpcState.SHOWING;
            mState = SpcState.CLOSED;
            if (wasShowing) {
                // Respond directly instead of through onCancel() so mInputProtector does not hold
                // back X, back, swipe, or outside taps.
                mResponseCallback.onResult(SpcResponseStatus.CANCEL);
            }
        }

        /*package*/ AdaptiveUiEntry getEntryForTesting() {
            return mEntry;
        }
    }

    private final SecurePaymentConfirmationView mView;
    private final PropertyModel mModel;
    private final Presenter mPresenter;
    private final Boolean mShowOptOut;
    private final Boolean mInformOnly;
    private final Callback<Integer> mResponseCallback;
    private final @SPCTransactionMode int mTransactionMode;
    private InputProtector mInputProtector = new InputProtector();
    private @SpcState int mState = SpcState.NOT_SHOWN;

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
        mShowOptOut = showOptOut;
        mInformOnly = informOnly;
        mResponseCallback = responseCallback;
        mTransactionMode = transactionMode;

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
                            assertNonNull(payeeOrigin), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
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
        Bitmap paymentIconBitmap = ((BitmapDrawable) paymentIcon).getBitmap();
        if (paymentIconBitmap == null) {
            paymentIcon =
                    ResourcesCompat.getDrawable(
                            context.getResources(), R.drawable.credit_card, context.getTheme());
        } else {
            // Need to round the corners of the provided payment icon. The target corner radius must
            // be scaled according to how large the original icon is compared to the target icon
            // dimensions.
            final int paymentIconTargetWidth =
                    context.getResources()
                            .getDimensionPixelSize(R.dimen.payments_instrument_icon_width);
            final int paymentIconTargetHeight =
                    context.getResources()
                            .getDimensionPixelSize(R.dimen.payments_instrument_icon_height);
            final int paymentIconCurrentWidth = paymentIconBitmap.getWidth();
            final int paymentIconCurrentHeight = paymentIconBitmap.getHeight();
            // We compute the scale factor this way to match the android:scaleType="fit-center"
            // attribute in payments_instrument.xml.
            final float paymentIconTargetScale =
                    Math.min(
                            (float) paymentIconTargetWidth / paymentIconCurrentWidth,
                            (float) paymentIconTargetHeight / paymentIconCurrentHeight);
            final int targetCornerRadius =
                    context.getResources()
                            .getDimensionPixelSize(R.dimen.payments_instrument_icon_corner_radius);
            final int scaledCornerRadius =
                    paymentIconTargetScale > 0
                            ? Math.round((float) targetCornerRadius / paymentIconTargetScale)
                            : 0;
            paymentIcon =
                    ViewUtils.createRoundedBitmapDrawable(
                            context.getResources(), paymentIconBitmap, scaledCornerRadius);
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

        String continueButtonLabel =
                mInformOnly
                        ? context.getString(R.string.payments_confirm_button)
                        : context.getString(
                                R.string.secure_payment_confirmation_verify_button_label);
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
                                continueButtonLabel)
                        .build();
        PropertyModelChangeProcessor.create(
                mModel, mView, SecurePaymentConfirmationViewBinder::bind);

        AdaptiveUiController adaptiveUiController = AdaptiveUiController.from(window);
        mPresenter =
                adaptiveUiController != null
                        ? new AdaptiveUiPresenter(
                                context, adaptiveUiController, continueButtonLabel)
                        : new BottomSheetPresenter(
                                assertNonNull(BottomSheetControllerProvider.from(window)));
    }

    /**
     * Requests to show the SPC UI. Returns true if shown and false otherwise. Returns false if
     * called again.
     */
    public boolean show() {
        if (mState != SpcState.NOT_SHOWN) return false;
        // A dismissal during mPresenter.show() means it isn't showing.
        if (!mPresenter.show() || mState != SpcState.NOT_SHOWN) return false;
        mState = SpcState.SHOWING;

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

    /** Hides the SPC UI. Sends no response. */
    public void hide() {
        // ChromePaymentRequestService.close() re-enters hide() inside the primary response, when
        // mState is CLOSING. Ignoring it keeps PRIMARY_ACTION as the dismiss reason.
        if (mState == SpcState.CLOSING || mState == SpcState.CLOSED) return;
        mState = SpcState.CLOSED;
        mPresenter.hide();
    }

    private void onContinue() {
        if (!mInputProtector.shouldInputBeProcessed()
                && mTransactionMode == SPCTransactionMode.NONE) {
            return;
        }
        if (mState != SpcState.SHOWING) return;

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
        if (mState != SpcState.SHOWING) return;

        hide();
        mResponseCallback.onResult(SpcResponseStatus.CANCEL);
    }

    private void onVerifyAnotherWay() {
        if (!mInputProtector.shouldInputBeProcessed()
                && mTransactionMode == SPCTransactionMode.NONE) {
            return;
        }
        if (mState != SpcState.SHOWING) return;

        hide();
        mResponseCallback.onResult(SpcResponseStatus.ANOTHER_WAY);
    }

    private void onOptOut() {
        if (!mInputProtector.shouldInputBeProcessed()
                && mTransactionMode == SPCTransactionMode.NONE) {
            return;
        }
        if (mState != SpcState.SHOWING) return;

        hide();
        mResponseCallback.onResult(SpcResponseStatus.OPT_OUT);
    }

    /**
     * Called by PaymentRequestTestBridge for cross-platform browser tests, the following methods
     * bypass the input protector. The Java unit tests simulate clicking the button and therefore
     * test the input protector.
     */
    public boolean cancelForTest() {
        if (mState != SpcState.SHOWING) return false;
        hide();
        mResponseCallback.onResult(SpcResponseStatus.CANCEL);
        return true;
    }

    public boolean optOutForTest() {
        if (!mShowOptOut) return false;
        if (mState != SpcState.SHOWING) return false;
        hide();
        mResponseCallback.onResult(SpcResponseStatus.OPT_OUT);
        return true;
    }

    /*package*/ Presenter getPresenterForTesting() {
        return mPresenter;
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
}
