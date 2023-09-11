// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import androidx.annotation.Nullable;

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

    public PlusAddressCreationPrompt(PlusAddressCreationDelegate delegate) {
        mPlusAddressDelegate = delegate;
        // TODO(crbug.com/1467623): Set a custom view, drop hard-coded strings etc. Keeping the
        // modal as simple as possible for now.
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.TITLE, "Lorem Ipsum")
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, "Yep")
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, "Dolor");
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
}
