// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.javascript_dialogs;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * The controller to communicate with native TabModalDialogViewAndroid for a tab modal JavaScript
 * dialog. This can be an alert dialog, a prompt dialog or a confirm dialog.
 */
@JNINamespace("javascript_dialogs")
public class JavascriptTabModalDialog extends JavascriptModalDialog {
    private long mNativeDialogPointer;

    /**
     * Constructor for initializing contents to be shown on the dialog.
     */
    private JavascriptTabModalDialog(
            String title, String message, String promptText, int negativeButtonTextId) {
        super(title, message, promptText, false, R.string.ok, negativeButtonTextId);
    }

    @CalledByNative
    private static JavascriptTabModalDialog createAlertDialog(String title, String message) {
        return new JavascriptTabModalDialog(title, message, null, 0);
    }

    @CalledByNative
    private static JavascriptTabModalDialog createConfirmDialog(String title, String message) {
        return new JavascriptTabModalDialog(title, message, null, R.string.cancel);
    }

    @CalledByNative
    private static JavascriptTabModalDialog createPromptDialog(
            String title, String message, String defaultPromptText) {
        return new JavascriptTabModalDialog(title, message, defaultPromptText, R.string.cancel);
    }

    @CalledByNative
    private void showDialog(WindowAndroid window, long nativeDialogPointer) {
        assert window != null;
        Context context = window.getContext().get();
        ModalDialogManager dialogManager = window.getModalDialogManager();
        // If the context has gone away, then just clean up the native pointer.
        if (context == null || dialogManager == null) {
            JavascriptTabModalDialogJni.get().cancel(
                    nativeDialogPointer, JavascriptTabModalDialog.this, false);
            return;
        }

        // Cache the native dialog pointer so that we can use it to return the response.
        mNativeDialogPointer = nativeDialogPointer;
        show(context, dialogManager, ModalDialogManager.ModalDialogType.TAB);
    }

    @CalledByNative
    private String getUserInput() {
        return mDialogCustomView.getPromptText();
    }

    @CalledByNative
    private void dismiss() {
        dismiss(DialogDismissalCause.DISMISSED_BY_NATIVE);
        mNativeDialogPointer = 0;
    }

    /**
     * Sends notification to native that the user accepts the dialog.
     * @param promptResult The text edited by user.
     */
    @Override
    protected void accept(String promptResult, boolean suppressDialogs) {
        if (mNativeDialogPointer == 0) return;
        JavascriptTabModalDialogJni.get().accept(
                mNativeDialogPointer, JavascriptTabModalDialog.this, promptResult);
    }

    /**
     * Sends notification to native that the user cancels the dialog.
     */
    @Override
    protected void cancel(boolean buttonClicked, boolean suppressDialogs) {
        if (mNativeDialogPointer == 0) return;
        JavascriptTabModalDialogJni.get().cancel(
                mNativeDialogPointer, JavascriptTabModalDialog.this, buttonClicked);
    }

    @NativeMethods
    interface Natives {
        void accept(long nativeTabModalDialogViewAndroid, JavascriptTabModalDialog caller,
                String prompt);
        void cancel(long nativeTabModalDialogViewAndroid, JavascriptTabModalDialog caller,
                boolean buttonClicked);
    }
}
