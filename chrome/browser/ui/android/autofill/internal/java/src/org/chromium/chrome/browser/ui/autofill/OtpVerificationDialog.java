// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;
import android.content.Context;
import android.os.Handler;
import android.text.Editable;
import android.text.SpannableString;
import android.text.TextWatcher;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import androidx.core.content.res.ResourcesCompat;

import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/** Dialog shown to the user for credit card unmasking using OTP-based verification. */
public class OtpVerificationDialog {
    private static final int ANIMATION_DURATION_MS = 250;

    /** Interface for the caller to be notified of user actions. */
    public interface Listener {
        /**
         * Notify that the user clicked on the positive button after entering the otp.
         *
         * @param otp The OTP entered by the user.
         */
        void onConfirm(String otp);
        /** Notify that a new otp was requested by the user. */
        void onNewOtpRequested();
        /** Notify the caller that the dialog was dismissed. */
        void onDialogDismissed();
    }

    private final ModalDialogProperties.Controller mModalDialogController =
            new ModalDialogProperties.Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {
                    switch (buttonType) {
                        case ModalDialogProperties.ButtonType.POSITIVE:
                            mListener.onConfirm(mOtpEditText.getText().toString());
                            showProgressBarOverlay();
                            break;
                        case ModalDialogProperties.ButtonType.NEGATIVE:
                            mModalDialogManager.dismissDialog(
                                    model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                            break;
                    }
                }

                @Override
                public void onDismiss(PropertyModel model, int dismissalCause) {
                    mListener.onDialogDismissed();
                }
            };

    private TextWatcher mOtpTextWatcher = new TextWatcher() {
        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
            // Hide the error message view if it is visible, as the user is editing the OTP.
            mOtpErrorMessageTextView.setVisibility(View.GONE);
            // Disable the positive button if the length of the text is not equal to the OTP length.
            mDialogModel.set(
                    ModalDialogProperties.POSITIVE_BUTTON_DISABLED, s.length() != mOtpLength);
        }
        @Override
        public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

        @Override
        public void afterTextChanged(Editable s) {}
    };

    private final Context mContext;
    private final Listener mListener;
    private final ModalDialogManager mModalDialogManager;
    private View mProgressBarOverlayView;
    private View mOtpVerificationDialogContents;
    private EditText mOtpEditText;
    private TextView mOtpErrorMessageTextView;
    private TextView mOtpResendMessageTextView;
    private PropertyModel mDialogModel;
    private int mOtpLength;

    public OtpVerificationDialog(
            Context context, Listener listener, ModalDialogManager modalDialogManager) {
        this.mContext = context;
        this.mListener = listener;
        this.mModalDialogManager = modalDialogManager;
    }

    /**
     * Show the OtpVerification dialog.
     *
     * @param otpLength The expected length of the OTP input field.
     */
    public void show(int otpLength) {
        mOtpLength = otpLength;
        View view = LayoutInflater.from(mContext).inflate(R.layout.otp_verification_dialog, null);
        mOtpEditText = view.findViewById(R.id.otp_input);
        mOtpErrorMessageTextView = view.findViewById(R.id.otp_error_message);
        mOtpResendMessageTextView = view.findViewById(R.id.otp_resend_message);
        mOtpVerificationDialogContents = view.findViewById(R.id.otp_verification_dialog_contents);
        mProgressBarOverlayView = view.findViewById(R.id.progress_bar_overlay);

        mOtpEditText.setHint(mContext.getResources().getString(
                R.string.autofill_payments_otp_verification_dialog_otp_input_hint, otpLength));
        mOtpEditText.addTextChangedListener(mOtpTextWatcher);
        mOtpErrorMessageTextView.setVisibility(View.GONE);

        mOtpResendMessageTextView.setText(getOtpResendMessage());
        mOtpResendMessageTextView.setMovementMethod(LinkMovementMethod.getInstance());

        mDialogModel = buildDialogModel(view);
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    private PropertyModel buildDialogModel(View customView) {
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, mModalDialogController)
                .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                .with(ModalDialogProperties.TITLE,
                        mContext.getResources().getString(
                                R.string.autofill_payments_otp_verification_dialog_title))
                .with(ModalDialogProperties.TITLE_ICON,
                        ResourcesCompat.getDrawable(mContext.getResources(),
                                R.drawable.google_pay_with_divider, mContext.getTheme()))
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                        mContext.getResources().getString(
                                R.string.autofill_payments_otp_verification_dialog_negative_button_label))
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                        mContext.getResources().getString(
                                R.string.autofill_payments_otp_verification_dialog_positive_button_label))
                .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true)
                .build();
    }

    private SpannableString getOtpResendMessage() {
        return SpanApplier.applySpans(
                mContext.getResources().getString(
                        R.string.autofill_payments_otp_verification_dialog_cant_find_code_message),
                new SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(mContext.getResources(), textView -> {
                            mOtpEditText.getText().clear();
                            mListener.onNewOtpRequested();
                        })));
    }

    /** Show an error message for the submitted otp. */
    public void showOtpErrorMessage(String errorMessage) {
        hideProgressBarOverlay();
        mOtpErrorMessageTextView.setVisibility(View.VISIBLE);
        mOtpErrorMessageTextView.setText(errorMessage);
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true);
    }

    /** Dismiss the dialog if is already showing. */
    public void dismissDialog() {
        mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    /** Show confirmation message  */
    public void showConfirmationAndDismissDialog(String confirmationMessage) {
        if (mProgressBarOverlayView != null) {
            mProgressBarOverlayView.findViewById(R.id.progress_bar).setVisibility(View.GONE);
            mProgressBarOverlayView.findViewById(R.id.confirmation_icon)
                    .setVisibility(View.VISIBLE);
            ((TextView) mProgressBarOverlayView.findViewById(R.id.progress_bar_message))
                    .setText(confirmationMessage);
        }
        Runnable dismissRunnable = () -> dismissDialog();
        new Handler().postDelayed(dismissRunnable, ANIMATION_DURATION_MS);
    }

    private void showProgressBarOverlay() {
        mProgressBarOverlayView.setVisibility(View.VISIBLE);
        mProgressBarOverlayView.setAlpha(0f);
        mProgressBarOverlayView.animate().alpha(1f).setDuration(ANIMATION_DURATION_MS);
        mOtpVerificationDialogContents.animate().alpha(0f).setDuration(ANIMATION_DURATION_MS);
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true);
    }

    private void hideProgressBarOverlay() {
        mProgressBarOverlayView.setVisibility(View.GONE);
        mProgressBarOverlayView.animate().alpha(0f).setDuration(ANIMATION_DURATION_MS);
        mOtpVerificationDialogContents.animate().alpha(1f).setDuration(ANIMATION_DURATION_MS);
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, false);
    }
}
