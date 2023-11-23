// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.ui.base.WindowAndroid;

/**
 * Delegate class for modal permission dialogs. Contains all of the data displayed in a prompt,
 * including the button strings, message text and the icon.
 *
 * This class is also the interface to the native-side permissions code. When the user responds to
 * the permission dialog, the decision is conveyed across the JNI so that the native code can
 * respond appropriately.
 */
@JNINamespace("permissions")
public class PermissionDialogDelegate {
    /** The native-side counterpart of this class */
    private long mNativeDelegatePtr;

    /** The controller for this class */
    private PermissionDialogController mDialogController;

    /** The window for which to create the dialog. */
    private WindowAndroid mWindow;

    /** The icon to display in the dialog. */
    private int mDrawableId;

    /** Text shown in the dialog. */
    private String mMessageText;

    /** Text shown on the primary button, e.g. "Allow". */
    private String mPrimaryButtonText;

    /** Text shown on the secondary button, e.g. "Block". */
    private String mSecondaryButtonText;

    /** The {@link ContentSettingsType}s requested in this dialog.  */
    private int[] mContentSettingsTypes;

    public WindowAndroid getWindow() {
        return mWindow;
    }

    public int[] getContentSettingsTypes() {
        return mContentSettingsTypes.clone();
    }

    public int getDrawableId() {
        return mDrawableId;
    }

    public String getMessageText() {
        return mMessageText;
    }

    public String getPrimaryButtonText() {
        return mPrimaryButtonText;
    }

    public String getSecondaryButtonText() {
        return mSecondaryButtonText;
    }

    public void onAccept() {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get().accept(mNativeDelegatePtr, PermissionDialogDelegate.this);
    }

    public void onCancel() {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get().cancel(mNativeDelegatePtr, PermissionDialogDelegate.this);
    }

    public void onDismiss() {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get()
                .dismissed(mNativeDelegatePtr, PermissionDialogDelegate.this);
    }

    public void destroy() {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get()
                .destroy(mNativeDelegatePtr, PermissionDialogDelegate.this);
        mNativeDelegatePtr = 0;
    }

    public void setDialogController(PermissionDialogController controller) {
        mDialogController = controller;
    }

    /** Return the size of the RequestType enum used for permission requests. */
    public static int getRequestTypeEnumSize() {
        return PermissionDialogDelegateJni.get().getRequestTypeEnumSize();
    }

    /** Called from C++ by |nativeDelegatePtr| to destroy the dialog. */
    @CalledByNative
    private void dismissFromNative() {
        assert mDialogController != null;
        mDialogController.dismissFromNative(this);
    }

    @CalledByNative
    private void updateIcon(Bitmap icon) {
        assert mDialogController != null;
        mDialogController.updateIcon(icon);
    }

    @CalledByNative
    private int getIconSizeInPx() {
        assert mDialogController != null;
        return mDialogController.getIconSizeInPx();
    }

    /**
     * Called from C++ by |nativeDelegatePtr| to instantiate this class.
     *
     * @param nativeDelegatePtr The native counterpart that this object owns.
     * @param window The window to create the dialog for.
     * @param contentSettingsTypes The content settings types requested by this dialog.
     * @param iconId The id of the icon to display in the dialog.
     * @param message The message to display in the dialog.
     * @param primaryTextButton The text to display on the primary button.
     * @param secondaryTextButton The text to display on the primary button.
     */
    @CalledByNative
    private static PermissionDialogDelegate create(
            long nativeDelegatePtr,
            WindowAndroid window,
            int[] contentSettingsTypes,
            int iconId,
            String message,
            String primaryButtonText,
            String secondaryButtonText) {
        return new PermissionDialogDelegate(
                nativeDelegatePtr,
                window,
                contentSettingsTypes,
                iconId,
                message,
                primaryButtonText,
                secondaryButtonText);
    }

    /** Upon construction, this class takes ownership of the passed in native delegate. */
    private PermissionDialogDelegate(
            long nativeDelegatePtr,
            WindowAndroid window,
            int[] contentSettingsTypes,
            int iconId,
            String message,
            String primaryButtonText,
            String secondaryButtonText) {
        mNativeDelegatePtr = nativeDelegatePtr;
        mWindow = window;
        mContentSettingsTypes = contentSettingsTypes;
        mDrawableId = iconId;
        mMessageText = message;
        mPrimaryButtonText = primaryButtonText;
        mSecondaryButtonText = secondaryButtonText;
    }

    @NativeMethods
    interface Natives {
        void accept(long nativePermissionDialogDelegate, PermissionDialogDelegate caller);

        void cancel(long nativePermissionDialogDelegate, PermissionDialogDelegate caller);

        void dismissed(long nativePermissionDialogDelegate, PermissionDialogDelegate caller);

        void destroy(long nativePermissionDialogDelegate, PermissionDialogDelegate caller);

        int getRequestTypeEnumSize();
    }
}
