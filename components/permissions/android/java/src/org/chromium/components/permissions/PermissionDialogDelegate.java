// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.graphics.Bitmap;

import androidx.core.util.Pair;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/**
 * Delegate class for modal permission dialogs. Contains all of the data displayed in a prompt,
 * including the button strings, message text and the icon.
 *
 * <p>This class is also the interface to the native-side permissions code. When the user responds
 * to the permission dialog, the decision is conveyed across the JNI so that the native code can
 * respond appropriately.
 */
@JNINamespace("permissions")
@NullMarked
public class PermissionDialogDelegate {
    /** The native-side counterpart of this class */
    private long mNativeDelegatePtr;

    /** The controller for this class */
    private @Nullable PermissionDialogController mDialogController;

    /** The window for which to create the dialog. */
    private final WindowAndroid mWindow;

    /** The icon to display in the dialog. */
    private int mDrawableId;

    /** Text shown in the dialog. */
    private String mMessageText;

    /** Text to display on the persistent grant button, e.g. "Allow". */
    private String mPositiveButtonText;

    /** Text The text to display on the persistent deny button, e.g. "Block". */
    private String mNegativeButtonText;

    /**
     * Text to display on the ephemeral grant button, e.g. "Allow this time". May be an empty string
     * in which case the button should not be shown.
     */
    private String mPositiveEphemeralButtonText;

    /** Whether to show the persistent grant button first, followed by the ephemeral option. */
    private boolean mShowPositiveNonEphemeralAsFirstButton;

    /** The {@link ContentSettingsType}s requested in this dialog. */
    private int[] mContentSettingsTypes;

    // Prompt(screen) variant we want to display on the dialog.
    private @EmbeddedPromptVariant int mEmbeddedPromptVariant;

    /**
     * Defines a (potentially empty) list of ranges represented as pairs of <startIndex, endIndex>,
     * which shall be used by the UI to format the specified ranges as bold text.
     */
    private final List<Pair<Integer, Integer>> mBoldedRanges = new ArrayList<>();

    public WindowAndroid getWindow() {
        return mWindow;
    }

    public int[] getContentSettingsTypes() {
        return mContentSettingsTypes.clone();
    }

    public boolean canShowEphemeralOption() {
        return !mPositiveEphemeralButtonText.isEmpty();
    }

    public int getDrawableId() {
        return mDrawableId;
    }

    public String getMessageText() {
        return mMessageText;
    }

    public List<Pair<Integer, Integer>> getBoldedRanges() {
        return mBoldedRanges;
    }

    public String getPositiveButtonText() {
        return mPositiveButtonText;
    }

    public String getNegativeButtonText() {
        return mNegativeButtonText;
    }

    public String getPositiveEphemeralButtonText() {
        return mPositiveEphemeralButtonText;
    }

    public boolean shouldShowPositiveNonEphemeralAsFirstButton() {
        return mShowPositiveNonEphemeralAsFirstButton;
    }

    public @EmbeddedPromptVariant int getEmbeddedPromptVariant() {
        return mEmbeddedPromptVariant;
    }

    public void setEmbeddedPromptVariant(@EmbeddedPromptVariant int variant) {
        mEmbeddedPromptVariant = variant;
    }

    public boolean isEmbeddedPromptVariant() {
        return mEmbeddedPromptVariant != EmbeddedPromptVariant.UNINITIALIZED;
    }

    public boolean isTablet() {
        return DeviceFormFactor.isWindowOnTablet(getWindow());
    }

    public void onAccept() {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get().accept(mNativeDelegatePtr);
    }

    public void onAcceptThisTime() {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get().acceptThisTime(mNativeDelegatePtr);
    }

    public void onDeny() {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get().deny(mNativeDelegatePtr);
    }

    public void onDismiss(@DismissalType int dismissalType) {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get().dismissed(mNativeDelegatePtr, dismissalType);
    }

    public void onAcknowledge() {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get().acknowledge(mNativeDelegatePtr);
    }

    public void onSystemPermissionResolved(boolean accepted) {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get().systemPermissionResolved(mNativeDelegatePtr, accepted);
    }

    public void onCloseButtonClicked() {
        assert mDialogController != null;
        mDialogController.dismissByCloseButton(this);
    }

    public void destroy() {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get().destroy(mNativeDelegatePtr);
        mNativeDelegatePtr = 0;
    }

    public void onSystemSettingsShown() {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get().systemSettingsShown(mNativeDelegatePtr);
    }

    public void onResume() {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get().resumed(mNativeDelegatePtr);
    }

    public void setDialogController(PermissionDialogController controller) {
        mDialogController = controller;
    }

    public @LocationAccuracy int getInitialGeolocationAccuracySelection() {
        assert mNativeDelegatePtr != 0;
        return PermissionDialogDelegateJni.get()
                .getInitialGeolocationAccuracySelection(mNativeDelegatePtr);
    }

    public void onGeolocationAccuracySelected(@LocationAccuracy int locationAccuracy) {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get()
                .onGeolocationAccuracySelected(mNativeDelegatePtr, locationAccuracy);
    }

    /** Return the size of the RequestType enum used for permission requests. */
    public static int getRequestTypeEnumSize() {
        return PermissionDialogDelegateJni.get().getRequestTypeEnumSize();
    }

    @CalledByNative
    private void dismissFromNative() {
        assert mDialogController != null;
        mDialogController.dismissFromNative(this);
    }

    @CalledByNative
    private void updateIcon(Bitmap icon) {
        assert mDialogController != null;
        mDialogController.updateIcon(this, icon);
    }

    @CalledByNative
    private int getIconSizeInPx() {
        assert mDialogController != null;
        return mDialogController.getIconSizeInPx(this);
    }

    @CalledByNative
    private void notifyPermissionAllowed() {
        assert mDialogController != null;
        mDialogController.notifyPermissionAllowed(this);
    }

    /**
     * Called from C++ by |nativeDelegatePtr| to instantiate this class.
     *
     * @param nativeDelegatePtr The native counterpart that this object owns.
     * @param window The window to create the dialog for.
     * @param contentSettingsTypes The content settings types requested by this dialog.
     * @param iconId The id of the icon to display in the dialog.
     * @param message The message to display in the dialog.
     * @param boldedRanges A list of ranges (pairs of <textOffset, rangeSize>) that should be
     *     formatted as bold in the message.
     * @param positiveButtonText The text to display on the persistent grant button.
     * @param negativeButtonText The text to display on the persistent deny button.
     * @param positiveEphemeralButtonText The text to display on the ephemeral grant button. May be
     *     empty in which case only persistent grant and deny buttons are shown.
     */
    @CalledByNative
    private static PermissionDialogDelegate create(
            long nativeDelegatePtr,
            WindowAndroid window,
            int[] contentSettingsTypes,
            int iconId,
            String message,
            int[] boldedRanges,
            String positiveButtonText,
            String negativeButtonText,
            String positiveEphemeralButtonText,
            boolean showPositiveNonEphemeralAsFirstButton,
            @EmbeddedPromptVariant int variant) {
        assert (boldedRanges.length % 2 == 0); // Contains a list of offset and length values

        return new PermissionDialogDelegate(
                nativeDelegatePtr,
                window,
                contentSettingsTypes,
                iconId,
                message,
                boldedRanges,
                positiveButtonText,
                negativeButtonText,
                positiveEphemeralButtonText,
                showPositiveNonEphemeralAsFirstButton,
                variant);
    }

    private PermissionDialogDelegate(
            long nativeDelegatePtr,
            WindowAndroid window,
            int[] contentSettingsTypes,
            int iconId,
            String message,
            int[] boldedRanges,
            String positiveButtonText,
            String negativeButtonText,
            String positiveEphemeralButtonText,
            boolean showPositiveNonEphemeralAsFirstButton,
            @EmbeddedPromptVariant int variant) {
        mNativeDelegatePtr = nativeDelegatePtr;
        mWindow = window;
        mContentSettingsTypes = contentSettingsTypes;
        mDrawableId = iconId;
        mMessageText = message;
        for (int i = 0; i + 1 < boldedRanges.length; i += 2) {
            mBoldedRanges.add(new Pair(boldedRanges[i], boldedRanges[i + 1]));
        }
        mPositiveButtonText = positiveButtonText;
        mNegativeButtonText = negativeButtonText;
        mPositiveEphemeralButtonText = positiveEphemeralButtonText;
        mShowPositiveNonEphemeralAsFirstButton = showPositiveNonEphemeralAsFirstButton;
        mEmbeddedPromptVariant = variant;
    }

    /** Called by native code to update the current permission dialog with new screen. */
    @CalledByNative
    void updateDialog(
            int[] contentSettingsTypes,
            int iconId,
            String message,
            int[] boldedRanges,
            String positiveButtonText,
            String negativeButtonText,
            String positiveEphemeralButtonText,
            boolean showPositiveNonEphemeralAsFirstButton,
            @EmbeddedPromptVariant int variant) {
        mContentSettingsTypes = contentSettingsTypes;
        mMessageText = message;
        mDrawableId = iconId;
        mBoldedRanges.clear();
        for (int i = 0; i + 1 < boldedRanges.length; i += 2) {
            mBoldedRanges.add(new Pair(boldedRanges[i], boldedRanges[i + 1]));
        }
        mPositiveButtonText = positiveButtonText;
        mNegativeButtonText = negativeButtonText;
        mPositiveEphemeralButtonText = positiveEphemeralButtonText;
        mShowPositiveNonEphemeralAsFirstButton = showPositiveNonEphemeralAsFirstButton;
        mEmbeddedPromptVariant = variant;

        assert mDialogController != null;
        mDialogController.updateDialog(this);
    }

    @NativeMethods
    interface Natives {
        void accept(long nativePermissionDialogDelegate);

        void acceptThisTime(long nativePermissionDialogDelegate);

        void acknowledge(long nativePermissionDialogDelegate);

        void deny(long nativePermissionDialogDelegate);

        void dismissed(long nativePermissionDialogDelegate, @DismissalType int dismissalType);

        void resumed(long nativePermissionDialogDelegate);

        void destroy(long nativePermissionDialogDelegate);

        void systemPermissionResolved(long nativePermissionDialogDelegate, boolean accept);

        void systemSettingsShown(long nativePermissionDialogDelegate);

        int getRequestTypeEnumSize();

        void onGeolocationAccuracySelected(
                long nativePermissionDialogDelegate, @LocationAccuracy int locationAccuracy);

        @LocationAccuracy
        int getInitialGeolocationAccuracySelection(long nativePermissionDialogDelegate);
    }
}
