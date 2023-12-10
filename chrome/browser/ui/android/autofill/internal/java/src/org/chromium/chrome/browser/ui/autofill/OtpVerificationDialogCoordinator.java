// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.ALL_KEYS;
import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.EDIT_TEXT_HINT;
import static org.chromium.chrome.browser.ui.autofill.OtpVerificationDialogProperties.OTP_LENGTH;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Optional;

/** The coordinator for the OTP Verification Dialog. Manages the sub-component objects. **/
class OtpVerificationDialogCoordinator {
    /** Interface for the caller to be notified of user actions. */
    interface Delegate {
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

    private final OtpVerificationDialogMediator mMediator;
    private final Context mContext;
    private final OtpVerificationDialogView mDialogView;

    /**
     * Creates the {@link OtpVerificationDialogCoordinator}.
     *
     * @param context The context of the window where the dialog will be displayed.
     * @param modalDialogManager The modal dialog manager of the window where the dialog will be
     *         displayed.
     * @param delegate The delegate to be called with results of interaction.
     */
    static OtpVerificationDialogCoordinator create(
            Context context, ModalDialogManager modalDialogManager, Delegate delegate) {
        OtpVerificationDialogView otpVerificationDialogView =
                (OtpVerificationDialogView)
                        LayoutInflater.from(context)
                                .inflate(
                                        org.chromium.chrome.browser.ui.autofill.internal.R.layout
                                                .otp_verification_dialog,
                                        null);
        return new OtpVerificationDialogCoordinator(
                context, modalDialogManager, otpVerificationDialogView, delegate);
    }

    /**
     * Internal constructor for {@link OtpVerificationDialogCoordinator}. Used by tests to inject
     * parameters. External code should use OtpVerificationDialogCoordinator#create.
     *
     * @param context The context for accessing resources.
     * @param modalDialogManager The ModalDialogManager to display the dialog.
     * @param dialogView The custom view with dialog content.
     * @param delegate The delegate to be called with results of interaction.
     */
    @VisibleForTesting
    OtpVerificationDialogCoordinator(
            Context context,
            ModalDialogManager modalDialogManager,
            OtpVerificationDialogView dialogView,
            Delegate delegate) {
        mContext = context;
        mDialogView = dialogView;

        boolean useCustomTitleView =
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK);
        int titleIconId =
                useCustomTitleView ? R.drawable.google_pay : R.drawable.google_pay_with_divider;
        String title =
                mContext.getResources()
                        .getString(R.string.autofill_card_unmask_otp_input_dialog_title);

        if (useCustomTitleView) {
            ViewStub stub = mDialogView.findViewById(R.id.title_with_icon_stub);
            stub.setLayoutResource(R.layout.icon_after_title_view);
            stub.inflate();
        }
        PropertyModel.Builder dialogModelBuilder = getModalDialogModelBuilder(mDialogView);
        updateTitleView(useCustomTitleView, title, titleIconId, dialogModelBuilder);

        mMediator =
                new OtpVerificationDialogMediator(modalDialogManager, dialogModelBuilder, delegate);
    }

    /**
     * Show the OtpVerification dialog.
     *
     * @param otpLength The expected length of the OTP input field.
     */
    void show(int otpLength) {
        PropertyModel otpVerificationDialogModel = buildOtpVerificationDialogModel(otpLength);
        PropertyModelChangeProcessor.create(
                otpVerificationDialogModel, mDialogView, OtpVerificationDialogViewBinder::bind);
        mMediator.show(otpVerificationDialogModel);
    }

    /**
     * Show an error message for the submitted otp.
     *
     * @param errorMessage The string that is displayed in the error message.
     */
    void showOtpErrorMessage(String errorMessage) {
        mMediator.showOtpErrorMessage(Optional.of(errorMessage));
    }

    /** Dismiss the dialog if it is already showing. */
    void dismissDialog() {
        mMediator.dismissDialog();
    }

    /**
     * Show the confirmation message and dismiss the dialog. This is called once we receive a
     * successful server response when the user enters an OTP into the edit text field and clicks
     * accept.
     *
     * @param confirmationMessage The confirmation message to be shown.
     */
    void showConfirmationAndDismissDialog(String confirmationMessage) {
        mMediator.showConfirmationAndDismissDialog(confirmationMessage);
    }

    /**
     * Builds the dialog view model.
     *
     * @param otpLength The only non-static state of the dialog, needs to be passed in so that it
     * can be added to the model.
     */
    private PropertyModel buildOtpVerificationDialogModel(int otpLength) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(OTP_LENGTH, otpLength)
                .with(
                        EDIT_TEXT_HINT,
                        mContext.getResources()
                                .getString(
                                        R.string
                                                .autofill_payments_otp_verification_dialog_otp_input_hint,
                                        otpLength))
                .build();
    }

    /**
     * Gets the dialog model builder. {@link OtpVerificationDialogMediator} implements the
     * controller, so the final model will be finished being built in the Mediator.
     *
     * @param customView The view for the dialog model.
     */
    private PropertyModel.Builder getModalDialogModelBuilder(View customView) {
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                .with(
                        ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                        mContext.getResources()
                                .getString(
                                        org.chromium.chrome.browser.ui.autofill.internal.R.string
                                                .autofill_payments_otp_verification_dialog_negative_button_label))
                .with(
                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                        mContext.getResources()
                                .getString(
                                        org.chromium.chrome.browser.ui.autofill.internal.R.string
                                                .autofill_payments_otp_verification_dialog_positive_button_label))
                .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true)
                .with(
                        ModalDialogProperties.BUTTON_STYLES,
                        ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE);
    }

    /**
     * Updates the title and icon view. If AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK
     * feature is enabled, sets title and icon in the customView otherwise uses
     * PropertyModel.Builder for title and icon.
     *
     * @param useCustomTitleView Indicates true/false to use custom title view.
     * @param title Title of the prompt dialog.
     * @param titleIcon Icon near the title.
     * @param builder The PropertyModel.Builder instance.
     */
    private void updateTitleView(
            boolean useCustomTitleView,
            String title,
            @DrawableRes int titleIcon,
            PropertyModel.Builder builder) {
        if (useCustomTitleView) {
            TextView titleView = (TextView) mDialogView.findViewById(R.id.title);
            titleView.setText(title);
            ImageView iconView = (ImageView) mDialogView.findViewById(R.id.title_icon);
            iconView.setImageResource(titleIcon);
        } else {
            builder.with(ModalDialogProperties.TITLE, title);
            builder.with(
                    ModalDialogProperties.TITLE_ICON,
                    ResourcesCompat.getDrawable(
                            mContext.getResources(), titleIcon, mContext.getTheme()));
        }
    }
}
