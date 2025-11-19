// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.Editable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.ui.widget.TextViewWithLeading;

/** A JNI bridge to interact with the extension install dialog. */
@JNINamespace("extensions")
@NullMarked
public class ExtensionInstallDialogBridge implements ModalDialogProperties.Controller {
    private final long mNativeExtensionInstallDialogView;
    private final ModalDialogManager mModalDialogManager;
    private final Context mContext;
    private final PropertyModel.Builder mPropertyModelBuilder;
    private @Nullable PropertyModel mDialogModel;
    private @Nullable TextInputEditText mJustificationInputText;

    @VisibleForTesting
    public ExtensionInstallDialogBridge(
            long nativeExtensionInstallDialogView,
            Context context,
            ModalDialogManager modalDialogManager) {
        this.mNativeExtensionInstallDialogView = nativeExtensionInstallDialogView;
        this.mModalDialogManager = modalDialogManager;
        this.mContext = context;
        this.mPropertyModelBuilder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this);
    }

    /**
     * Create an instance of the {@link ExtensionInstallDialogBridge}
     *
     * @param nativeExtensionInstallDialogView The pointer to the native object.
     * @param windowAndroid The current {@link WindowAndroid} object.
     */
    @CalledByNative
    private static @Nullable ExtensionInstallDialogBridge create(
            long nativeExtensionInstallDialogView, WindowAndroid windowAndroid) {
        Context context = windowAndroid.getActivity().get();
        ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        if (context == null || modalDialogManager == null) {
            return null;
        }
        return new ExtensionInstallDialogBridge(
                nativeExtensionInstallDialogView, context, modalDialogManager);
    }

    /**
     * Adds title and buttons to the dialog's property model.
     *
     * @param title Text for the title.
     * @param iconBitmap Icon for the title.
     * @param acceptButtonLabel Label for the positive button which acts as the accept button.
     * @param cancelButtonLabel Label for the negative button which acts as the cancel button.
     */
    @CalledByNative
    public void withTitleAndButtons(
            String title, Bitmap iconBitmap, String acceptButtonLabel, String cancelButtonLabel) {
        Drawable iconDrawable = new BitmapDrawable(mContext.getResources(), iconBitmap);
        mPropertyModelBuilder
                .with(ModalDialogProperties.TITLE, title)
                .with(ModalDialogProperties.TITLE_ICON, iconDrawable)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, acceptButtonLabel)
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, cancelButtonLabel);
    }

    /**
     * Adds the custom view container with permissions and/or justifications to the dialog's
     * property model.
     *
     * @param heading Optional text for the permissions heading
     * @param permissionsText List of permissions information.
     * @param permissionsDetails List of permissions details, which may be empty.
     * @param heading Optional text for the justifications heading.
     * @param placeholderText Optional text for the justifications text area placeholder.
     */
    @CalledByNative
    public void withCustomView(
            String permissionsHeading,
            String[] permissionsText,
            String[] permissionsDetails,
            String justificationHeading,
            String justificationPlaceholderText) {
        View contentView =
                LayoutInflater.from(mContext).inflate(R.layout.extension_install_dialog, null);
        LinearLayout scrollViewContainer = contentView.findViewById(R.id.scroll_view_container);

        if (permissionsHeading.length() > 0) {
            LinearLayout permissionsContainer =
                    scrollViewContainer.findViewById(R.id.permissions_container);
            TextView permissionsHeadingView =
                    permissionsContainer.findViewById(R.id.permissions_heading);
            permissionsHeadingView.setText(permissionsHeading);

            LayoutInflater inflater = LayoutInflater.from(mContext);
            for (String permissionText : permissionsText) {
                TextViewWithLeading permissionTextView =
                        (TextViewWithLeading)
                                inflater.inflate(
                                        R.layout.modal_dialog_paragraph_view,
                                        permissionsContainer,
                                        false);
                permissionTextView.setText(permissionText);
                permissionsContainer.addView(permissionTextView);
                // TODO(crbug.com/424010795): Add permissionsDetails as a collapsible view, if
                // existent.
            }

            permissionsContainer.setVisibility(View.VISIBLE);
        }

        if (justificationHeading.length() > 0) {
            LinearLayout justificationContainer =
                    scrollViewContainer.findViewById(R.id.justification_container);
            TextView justificationHeadingView =
                    justificationContainer.findViewById(R.id.justification_heading);
            justificationHeadingView.setText(justificationHeading);

            TextInputLayout justificationInputLayout =
                    justificationContainer.findViewById(R.id.justification_input_layout);
            justificationInputLayout.setHint(justificationPlaceholderText);

            mJustificationInputText =
                    justificationContainer.findViewById(R.id.justification_input_text);
            mJustificationInputText.addTextChangedListener(
                    new EmptyTextWatcher() {
                        @Override
                        public void afterTextChanged(Editable s) {
                            int maxInputLength =
                                    mContext.getResources()
                                            .getInteger(
                                                    R.integer
                                                            .extension_install_dialog_justification_max_input);
                            boolean isTextTooLong = s.length() > maxInputLength;
                            setPositiveButtonDisabled(isTextTooLong);
                        }
                    });

            justificationContainer.setVisibility(View.VISIBLE);
        }

        mPropertyModelBuilder
                .with(ModalDialogProperties.CUSTOM_VIEW, contentView)
                .with(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE, true);
    }

    /** Shows the extension install dialog. */
    @CalledByNative
    public void showDialog() {
        mDialogModel = mPropertyModelBuilder.build();
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        ModalDialogManager modalDialogManager = assumeNonNull(mModalDialogManager);
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            modalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        } else {
            modalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                String justificationText = "";
                if (mJustificationInputText != null) {
                    Editable text = mJustificationInputText.getText();
                    if (text != null) {
                        justificationText = text.toString().trim();
                    }
                }
                ExtensionInstallDialogBridgeJni.get()
                        .onDialogAccepted(mNativeExtensionInstallDialogView, justificationText);
                break;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                ExtensionInstallDialogBridgeJni.get()
                        .onDialogCanceled(mNativeExtensionInstallDialogView);
                break;
            default:
                ExtensionInstallDialogBridgeJni.get()
                        .onDialogDismissed(mNativeExtensionInstallDialogView);
                break;
        }
        ExtensionInstallDialogBridgeJni.get().destroy(mNativeExtensionInstallDialogView);
    }

    /** Enables or disables the positive button in the dialog. */
    private void setPositiveButtonDisabled(boolean disabled) {
        // This method can only run AFTER the dialog model has been built.
        // If mDialogModel is null, we can't update the button state yet.
        if (mDialogModel == null) return;

        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, disabled);
    }

    @NativeMethods
    interface Natives {
        void onDialogAccepted(
                long nativeExtensionInstallDialogViewAndroid, String justificationText);

        void onDialogCanceled(long nativeExtensionInstallDialogViewAndroid);

        void onDialogDismissed(long nativeExtensionInstallDialogViewAndroid);

        void destroy(long nativeExtensionInstallDialogViewAndroid);
    }
}
