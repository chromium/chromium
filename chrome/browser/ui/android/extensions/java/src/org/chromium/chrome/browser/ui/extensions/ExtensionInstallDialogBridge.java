// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.VisibleForTesting;

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

/** A JNI bridge to interact with the extension install dialog. */
@JNINamespace("extensions")
@NullMarked
public class ExtensionInstallDialogBridge {
    private final long mNativeExtensionInstallDialogView;
    private final ModalDialogManager mModalDialogManager;
    private final Context mContext;
    private @Nullable PropertyModel mDialogModel;

    private static final String TAG = "ExtensionInstallDialogBridge";

    private final ModalDialogProperties.Controller mModalDialogController =
            new ModalDialogProperties.Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {
                    ModalDialogManager modalDialogManager = assumeNonNull(mModalDialogManager);
                    if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                        modalDialogManager.dismissDialog(
                                model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    } else {
                        modalDialogManager.dismissDialog(
                                model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                    }
                }

                @Override
                public void onDismiss(PropertyModel model, int dismissalCause) {
                    switch (dismissalCause) {
                        case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                            ExtensionInstallDialogBridgeJni.get()
                                    .onDialogAccepted(mNativeExtensionInstallDialogView);
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
                    ExtensionInstallDialogBridgeJni.get()
                            .destroy(mNativeExtensionInstallDialogView);
                }
            };

    @VisibleForTesting
    public ExtensionInstallDialogBridge(
            long nativeExtensionInstallDialogView,
            Context context,
            ModalDialogManager modalDialogManager) {
        this.mNativeExtensionInstallDialogView = nativeExtensionInstallDialogView;
        this.mModalDialogManager = modalDialogManager;
        this.mContext = context;
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
     * Shows the extension install dialog.
     *
     * @param acceptButtonLabel Label for the positive button which acts as the accept button.
     * @param cancelButtonLabel Label for the negative button which acts as the cancel button.
     */
    @CalledByNative
    public void showDialog(
            String title, Bitmap iconBitmap, String acceptButtonLabel, String cancelButtonLabel) {
        View extensionInstallDialogContentView =
                LayoutInflater.from(mContext).inflate(R.layout.extension_install_dialog, null);
        Drawable iconDrawable = new BitmapDrawable(mContext.getResources(), iconBitmap);

        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, mModalDialogController)
                        .with(ModalDialogProperties.TITLE, title)
                        .with(ModalDialogProperties.TITLE_ICON, iconDrawable)
                        .with(ModalDialogProperties.CUSTOM_VIEW, extensionInstallDialogContentView)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, acceptButtonLabel)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, cancelButtonLabel);
        mDialogModel = builder.build();
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    @NativeMethods
    interface Natives {
        void onDialogAccepted(long nativeExtensionInstallDialogViewAndroid);

        void onDialogCanceled(long nativeExtensionInstallDialogViewAndroid);

        void onDialogDismissed(long nativeExtensionInstallDialogViewAndroid);

        void destroy(long nativeExtensionInstallDialogViewAndroid);
    }
}
