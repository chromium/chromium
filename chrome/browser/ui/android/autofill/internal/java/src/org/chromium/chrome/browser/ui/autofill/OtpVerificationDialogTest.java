// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.text.SpannableString;
import android.text.style.ClickableSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

import java.util.Optional;

/**
 * Unit tests for {@link OtpVerificationDialogView}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class OtpVerificationDialogTest {
    private static final String ERROR_MESSAGE = "Error message";
    private static final String VALID_OTP = "123456";

    private OtpVerificationDialogView mOtpVerificationDialogView;
    private FakeModalDialogManager mModalDialogManager;
    private OtpVerificationDialogCoordinator mOtpVerificationDialogCoordinator;

    @Mock
    private OtpVerificationDialogCoordinator.Delegate mDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.TAB);
        mOtpVerificationDialogView = (OtpVerificationDialogView) LayoutInflater
                                             .from(ApplicationProvider.getApplicationContext())
                                             .inflate(R.layout.otp_verification_dialog, null);
        mOtpVerificationDialogCoordinator =
                new OtpVerificationDialogCoordinator(ApplicationProvider.getApplicationContext(),
                        mModalDialogManager, mOtpVerificationDialogView, mDelegate);
    }

    @Test
    public void testDefaultState() {
        int otpLength = 6;

        mOtpVerificationDialogCoordinator.show(otpLength);

        PropertyModel model = mModalDialogManager.getShownDialogModel();
        View view = model.get(ModalDialogProperties.CUSTOM_VIEW);
        // Verify that the error message is not shown.
        assertThat(view.findViewById(R.id.otp_error_message).getVisibility()).isEqualTo(View.GONE);
        // Verify that the hint shown in the OTP input field contains the value of the otpLength set
        // above.
        assertThat(((EditText) view.findViewById(R.id.otp_input)).getHint())
                .isEqualTo(ApplicationProvider.getApplicationContext().getString(
                        R.string.autofill_payments_otp_verification_dialog_otp_input_hint,
                        otpLength));
        // Verify that the positive button is disabled.
        assertThat(model.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED)).isTrue();
    }

    @Test
    public void testShowHideErrorMessage() {
        mOtpVerificationDialogCoordinator.show(/*otpLength=*/6);
        mOtpVerificationDialogView.showOtpErrorMessage(Optional.of(ERROR_MESSAGE));

        PropertyModel model = mModalDialogManager.getShownDialogModel();
        View view = model.get(ModalDialogProperties.CUSTOM_VIEW);
        // Verify that the error message is shown.
        TextView errorMessageTextView = (TextView) view.findViewById(R.id.otp_error_message);
        assertThat(errorMessageTextView.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(errorMessageTextView.getText()).isEqualTo(ERROR_MESSAGE);

        // Verify that editing the error test, hides the error message.
        EditText otpInputEditText = (EditText) view.findViewById(R.id.otp_input);
        otpInputEditText.setText("123");
        assertThat(errorMessageTextView.getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void testPositiveButtonDisabledState() {
        mOtpVerificationDialogCoordinator.show(/*otpLength=*/6);
        PropertyModel model = mModalDialogManager.getShownDialogModel();
        View view = model.get(ModalDialogProperties.CUSTOM_VIEW);

        EditText otpInputEditText = (EditText) view.findViewById(R.id.otp_input);

        // Verify that the positive button is disabled for input length < otpLength.
        otpInputEditText.setText("123");
        assertThat(model.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED)).isTrue();

        // Verify that the positive button is enabled for input length == otpLength.
        otpInputEditText.setText("123456");
        assertThat(model.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED)).isFalse();

        // Verify that the positive button is disabled for input length > otpLength.
        otpInputEditText.setText("1234567");
        assertThat(model.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED)).isTrue();
    }

    @Test
    public void testOtpSubmission() {
        mOtpVerificationDialogCoordinator.show(/*otpLength=*/6);
        PropertyModel model = mModalDialogManager.getShownDialogModel();
        View view = model.get(ModalDialogProperties.CUSTOM_VIEW);
        EditText otpInputEditText = (EditText) view.findViewById(R.id.otp_input);
        otpInputEditText.setText(VALID_OTP);

        mModalDialogManager.clickPositiveButton();

        // Verify that the listener is called with the text entered in the OTP input field.
        verify(mDelegate, times(1)).onConfirm(VALID_OTP);
        // Verify that the progress bar is shown.
        assertThat(view.findViewById(R.id.progress_bar_overlay).getVisibility())
                .isEqualTo(View.VISIBLE);
    }

    @Test
    public void testGetNewCode() {
        mOtpVerificationDialogCoordinator.show(/*otpLength=*/6);
        PropertyModel model = mModalDialogManager.getShownDialogModel();
        View view = model.get(ModalDialogProperties.CUSTOM_VIEW);
        TextView otpResendMessageTextView = (TextView) view.findViewById(R.id.otp_resend_message);
        SpannableString otpResendMessage = (SpannableString) otpResendMessageTextView.getText();
        ClickableSpan getNewCodeSpan =
                otpResendMessage.getSpans(0, otpResendMessage.length(), ClickableSpan.class)[0];

        getNewCodeSpan.onClick(otpResendMessageTextView);

        verify(mDelegate, times(1)).onNewOtpRequested();
    }

    @Test
    public void testDialogDismissal() {
        mOtpVerificationDialogCoordinator.show(/*otpLength=*/6);

        mModalDialogManager.dismissDialog(mModalDialogManager.getShownDialogModel(),
                DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);

        verify(mDelegate, times(1)).onDialogDismissed();
    }
}
