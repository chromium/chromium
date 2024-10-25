// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.ui.autofill.data.AuthenticatorOption;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Dialog that presents {@link AuthenticatorOption}s to the user to choose from for fetching credit
 * card information from the backend.
 */
public class AuthenticatorSelectionDialog implements AuthenticatorOptionsAdapter.ItemClickListener {
    private static final int ANIMATION_DURATION_MS = 250;

    /** Interface for the caller to be notified of user actions. */
    public interface Listener {
        /** Notify that the user selected an authenticator option. */
        void onOptionSelected(String authenticatorOptionIdentifier);

        /** Notify that the dialog was dismissed. */
        void onDialogDismissed();
    }

    private final ModalDialogProperties.Controller mModalDialogController =
            new ModalDialogProperties.Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {
                    switch (buttonType) {
                        case ModalDialogProperties.ButtonType.POSITIVE:
                            mListener.onOptionSelected(
                                    mSelectedAuthenticatorOption.getIdentifier());
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

    private final Context mContext;
    private final Listener mListener;
    private final ModalDialogManager mModalDialogManager;
    private View mAuthenticatorSelectionDialogView;
    private View mProgressBarOverlayView;
    private View mAuthenticatorSelectionDialogContentsView;
    private RecyclerView mAuthenticationOptionsRecyclerView;
    private AuthenticatorOptionsAdapter mAuthenticatorOptionsAdapter;

    private PropertyModel mDialogModel;
    private AuthenticatorOption mSelectedAuthenticatorOption;

    public AuthenticatorSelectionDialog(
            Context context, Listener listener, ModalDialogManager modalDialogManager) {
        this.mContext = context;
        this.mListener = listener;
        this.mModalDialogManager = modalDialogManager;
    }

    @Override
    public void onItemClicked(AuthenticatorOption option) {
        mSelectedAuthenticatorOption = option;
        mDialogModel.set(
                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                getPositiveButtonText(mSelectedAuthenticatorOption.getType()));
    }

    private String getPositiveButtonText(
            @CardUnmaskChallengeOptionType int authenticatorOptionType) {
        switch (authenticatorOptionType) {
            case CardUnmaskChallengeOptionType.SMS_OTP:
            case CardUnmaskChallengeOptionType.EMAIL_OTP:
                return mContext.getString(
                        R.string
                                .autofill_card_unmask_authentication_selection_dialog_ok_button_label_send);
            case CardUnmaskChallengeOptionType.CVC:
                return mContext.getString(
                        R.string
                                .autofill_card_unmask_authentication_selection_dialog_ok_button_label_continue);
            case CardUnmaskChallengeOptionType.UNKNOWN_TYPE:
                // This will never happen.
                assert false
                        : "Attempted to get positive button text for an authenticator option with"
                                + " Unknown type.";
        }
        return "";
    }

    /**
     * Shows an Authenticator Selection dialog.
     *
     * @param authenticatorOptions The authenticator options available to the user.
     */
    public void show(List<AuthenticatorOption> authenticatorOptions) {
        // By default, the first option will be selected.
        mSelectedAuthenticatorOption = authenticatorOptions.get(0);
        mAuthenticatorSelectionDialogView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.authenticator_selection_dialog, null);

        String title =
                mContext.getString(
                        authenticatorOptions.size() > 1
                                ? R.string
                                        .autofill_card_auth_selection_dialog_title_multiple_options
                                : R.string.autofill_card_unmask_verification_title);
        ViewStub title_view_stub =
                mAuthenticatorSelectionDialogView.findViewById(R.id.title_with_icon_stub);
        title_view_stub.setLayoutResource(R.layout.icon_after_title_view);
        title_view_stub.inflate();
        TextView titleView = (TextView) mAuthenticatorSelectionDialogView.findViewById(R.id.title);
        titleView.setText(title);
        ImageView iconView =
                (ImageView) mAuthenticatorSelectionDialogView.findViewById(R.id.title_icon);
        iconView.setImageResource(R.drawable.google_pay);

        mAuthenticatorSelectionDialogContentsView =
                mAuthenticatorSelectionDialogView.findViewById(
                        R.id.authenticator_selection_dialog_contents);
        mProgressBarOverlayView =
                mAuthenticatorSelectionDialogView.findViewById(R.id.progress_bar_overlay);
        mProgressBarOverlayView.setVisibility(View.GONE);
        // Set up the recycler view.
        mAuthenticationOptionsRecyclerView =
                (RecyclerView)
                        mAuthenticatorSelectionDialogView.findViewById(
                                R.id.authenticator_options_view);
        mAuthenticatorOptionsAdapter =
                new AuthenticatorOptionsAdapter(mContext, authenticatorOptions, this);
        mAuthenticationOptionsRecyclerView.setAdapter(mAuthenticatorOptionsAdapter);
        // Set up the ModalDialog.
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, mModalDialogController)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mAuthenticatorSelectionDialogView)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mContext.getString(
                                        R.string
                                                .autofill_payments_authenticator_selection_dialog_negative_button_label))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                getPositiveButtonText(mSelectedAuthenticatorOption.getType()))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE);
        mDialogModel = builder.build();
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    /**
     * Dismisses the Authenticator Selection dialog.
     *
     * @param cause The dialog dismissal cause.
     */
    public void dismiss(int cause) {
        mModalDialogManager.dismissDialog(mDialogModel, cause);
    }

    private void showProgressBarOverlay() {
        mProgressBarOverlayView.setVisibility(View.VISIBLE);
        mProgressBarOverlayView.setAlpha(0f);
        mProgressBarOverlayView.animate().alpha(1f).setDuration(ANIMATION_DURATION_MS);
        mAuthenticatorSelectionDialogContentsView
                .animate()
                .alpha(0f)
                .setDuration(ANIMATION_DURATION_MS);
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true);
    }
}
