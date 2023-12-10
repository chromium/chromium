// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.ANIMATION_DURATION_MS;

import android.content.Context;
import android.text.SpannableString;
import android.text.TextWatcher;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.view.View;
import android.widget.EditText;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.ViewDelegate;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.util.Optional;

/** Dialog shown to the user for credit card unmasking using OTP-based verification. */
public class OtpVerificationDialogView extends RelativeLayout {
    private View mProgressBarOverlayView;
    private View mOtpVerificationDialogViewContents;
    private EditText mOtpEditText;
    private TextView mOtpErrorMessageTextView;
    private TextView mOtpResendMessageTextView;

    public OtpVerificationDialogView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mProgressBarOverlayView = findViewById(R.id.progress_bar_overlay);
        mProgressBarOverlayView.setVisibility(View.GONE);
        mOtpVerificationDialogViewContents = findViewById(R.id.otp_verification_dialog_contents);
        mOtpEditText = findViewById(R.id.otp_input);
        mOtpErrorMessageTextView = findViewById(R.id.otp_error_message);
        mOtpErrorMessageTextView.setVisibility(View.GONE);
        mOtpResendMessageTextView = findViewById(R.id.otp_resend_message);
        mOtpResendMessageTextView.setMovementMethod(LinkMovementMethod.getInstance());
    }

    /**
     * Sets the edit text hint, which lets the user know how many digits are in the OTP.
     *
     * @param editTextHint The edit text hint, this hint will be dynamic based on the length of the
     * OTP.
     */
    void setEditTextHint(String editTextHint) {
        mOtpEditText.setHint(editTextHint);
    }

    void clearEditText() {
        mOtpEditText.getText().clear();
    }

    /**
     * Updates the necessary views and the model once a viewDelegate is received.
     *
     * @param viewDelegate The view delegate for this specific view.
     */
    void setViewDelegate(ViewDelegate viewDelegate) {
        mOtpEditText.addTextChangedListener(buildTextWatcher(viewDelegate));
        mOtpResendMessageTextView.setText(buildOtpResendMessageLink(getContext(), viewDelegate));
    }

    /**
     * Fades in the progress bar overlay for the dialog. This method is called once the users clicks
     * the accept button.
     */
    void fadeInProgressBarOverlay() {
        mProgressBarOverlayView.setVisibility(View.VISIBLE);
        mProgressBarOverlayView.setAlpha(0f);
        mProgressBarOverlayView.animate().alpha(1f).setDuration(ANIMATION_DURATION_MS);
        mOtpVerificationDialogViewContents.animate().alpha(0f).setDuration(ANIMATION_DURATION_MS);
    }

    /**
     * Fades out the progress bar overlay for the dialog. This method is called if the user receives
     * an error after submitting an OTP.
     */
    void fadeOutProgressBarOverlay() {
        mProgressBarOverlayView.setVisibility(View.GONE);
        mProgressBarOverlayView.animate().alpha(0f).setDuration(ANIMATION_DURATION_MS);
        mOtpVerificationDialogViewContents.animate().alpha(1f).setDuration(ANIMATION_DURATION_MS);
    }

    /**
     * Show an error message for the submitted OTP. This method is called once we receive an
     * unsuccessful server response after the user submits an OTP, and the errorMessage lets the
     * user know why the OTP was unsuccessful.
     *
     * @param errorMessage The error message that gets displayed to the user. Can be empty,
     * indicating there should be no error message shown on the dialog (so we hide it).
     */
    void showOtpErrorMessage(Optional<String> errorMessage) {
        mOtpErrorMessageTextView.setVisibility(View.VISIBLE);
        mOtpErrorMessageTextView.setText(errorMessage.get());
    }

    /**
     *  Hides the OTP error message. This method is called when the user changes the text in the
     *  edit text field while an OTP error message is showing.
     */
    void hideOtpErrorMessage() {
        mOtpErrorMessageTextView.setVisibility(View.GONE);
    }

    /**
     * Show a confirmation message for the submitted OTP. This method is called once we receive a
     * successful server response after the user submits an OTP.
     *
     * @param confirmationMessage The confirmation message displayed to the user.
     */
    void showConfirmation(String confirmationMessage) {
        assert mProgressBarOverlayView != null : "mProgressBarOverlayView is null.";
        mProgressBarOverlayView.findViewById(R.id.progress_bar).setVisibility(View.GONE);
        mProgressBarOverlayView.findViewById(R.id.confirmation_icon).setVisibility(View.VISIBLE);
        ((TextView) mProgressBarOverlayView.findViewById(R.id.progress_bar_message))
                .setText(confirmationMessage);
    }

    private TextWatcher buildTextWatcher(ViewDelegate viewDelegate) {
        return new EmptyTextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                viewDelegate.onTextChanged(s);
            }
        };
    }

    /** Builds Otp Resend Message Link **/
    private SpannableString buildOtpResendMessageLink(Context context, ViewDelegate viewDelegate) {
        return SpanApplier.applySpans(
                context.getResources()
                        .getString(
                                org.chromium.chrome.browser.ui.autofill.internal.R.string
                                        .autofill_payments_otp_verification_dialog_cant_find_code_message),
                new SpanInfo(
                        "<link>",
                        "</link>",
                        new NoUnderlineClickableSpan(
                                context,
                                textView -> {
                                    viewDelegate.onResendLinkClicked();
                                })));
    }
}
