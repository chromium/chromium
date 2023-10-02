// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A prompt to inform the user about plus address creation. Displayed as a modal
 * via `ModalDialogManager`.
 */
public class PlusAddressCreationPrompt implements ModalDialogProperties.Controller {
    private PropertyModel mDialogModel;
    private PlusAddressCreationDelegate mPlusAddressDelegate;
    private ModalDialogManager mModalDialogManager;
    private final View mDialogView;

    public PlusAddressCreationPrompt(PlusAddressCreationDelegate delegate, Activity activity,
            String primaryEmailAddressHolder, String modalTitle) {
        mPlusAddressDelegate = delegate;
        LayoutInflater inflater = LayoutInflater.from(activity);
        mDialogView = inflater.inflate(R.layout.plus_address_creation_prompt, null);

        // TODO(crbug.com/1467623): Switch to more-standard strings, without any notion of
        // inheriting the larger `generated_resources.grd`. This is a temporary state to work around
        // some project exigencies.
        Context context = ContextUtils.getApplicationContext();
        TextView modalTitleView = mDialogView.findViewById(R.id.plus_address_notice_title);
        modalTitleView.setText(modalTitle);
        TextView primaryEmailView = mDialogView.findViewById(R.id.plus_address_modal_primary_email);
        primaryEmailView.setText(primaryEmailAddressHolder);

        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mDialogView)
                        .with(ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getString(R.string.plus_address_modal_ok_text))
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                context.getString(R.string.plus_address_modal_cancel_text));
        mDialogModel = builder.build();
    }

    /**
     * Handles clicks of the buttons on the modal. Calls the delegate to inform
     * the C++ side.
     *
     * @param model the currently displayed model
     * @param buttonType the button click type (positive/negative)
     */
    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            mPlusAddressDelegate.onConfirmed();
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
            mPlusAddressDelegate.onCanceled();
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    /**
     * Handles dismissal of the modal. Informs the C++ side.
     *
     * @param model the currently displayed model
     * @param dismissalCause the reason for dismissal (e.g., negative button clicked)
     */
    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        mPlusAddressDelegate.onPromptDismissed();
    }

    /**
     * Shows the modal.
     *
     * @param modalDialogManager the manager that controls modals in clank.
     */
    public void show(@Nullable ModalDialogManager modalDialogManager) {
        if (modalDialogManager == null) {
            return;
        }
        mModalDialogManager = modalDialogManager;
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    View getDialogViewForTesting() {
        return mDialogView;
    }
}
