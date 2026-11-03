// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Bitmap;

import androidx.core.util.Pair;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
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
    @JniType("::permissions::PermissionDialogDelegate")
    interface NativePermissionDialogDelegate extends JniTypeToken {}

    /** The native-side counterpart of this class */
    private @Nullable JniRawPtr<NativePermissionDialogDelegate> mNativeDelegatePtr;

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

    public @GeolocationPromptType int getGeolocationPromptType() {
        return PermissionDialogDelegateJni.get()
                .getGeolocationPromptType(assumeNonNull(mNativeDelegatePtr));
    }

    public boolean isTablet() {
        return DeviceFormFactor.isWindowOnTablet(getWindow());
    }

    public void onAccept() {
        assert mNativeDelegatePtr != null;
        PermissionDialogDelegateJni.get().accept(mNativeDelegatePtr);
    }

    public void onAcceptThisTime() {
        assert mNativeDelegatePtr != null;
        PermissionDialogDelegateJni.get().acceptThisTime(mNativeDelegatePtr);
    }

    public void onDeny() {
        assert mNativeDelegatePtr != null;
        PermissionDialogDelegateJni.get().deny(mNativeDelegatePtr);
    }

    public void onDismiss(@DismissalType int dismissalType) {
        assert mNativeDelegatePtr != null;
        PermissionDialogDelegateJni.get().dismissed(mNativeDelegatePtr, dismissalType);
    }

    public void onAcknowledge() {
        assert mNativeDelegatePtr != null;
        PermissionDialogDelegateJni.get().acknowledge(mNativeDelegatePtr);
    }

    public void onSystemPermissionResolved(boolean accepted) {
        assert mNativeDelegatePtr != null;
        PermissionDialogDelegateJni.get().systemPermissionResolved(mNativeDelegatePtr, accepted);
    }

    public void onCloseButtonClicked() {
        assert mDialogController != null;
        mDialogController.dismissByCloseButton(this);
    }

    public void destroy() {
        assert mNativeDelegatePtr != null;
        PermissionDialogDelegateJni.get().destroy(mNativeDelegatePtr);
        clearNativePtr();
    }

    @CalledByNative
    void clearNativePtr() {
        if (mNativeDelegatePtr != null) {
            mNativeDelegatePtr.release();
            mNativeDelegatePtr = null;
        }
    }

    public void onSystemSettingsShown() {
        assert mNativeDelegatePtr != null;
        PermissionDialogDelegateJni.get().systemSettingsShown(mNativeDelegatePtr);
    }

    public void onResume() {
        assert mNativeDelegatePtr != null;
        PermissionDialogDelegateJni.get().resumed(mNativeDelegatePtr);
    }

    public void setDialogController(PermissionDialogController controller) {
        mDialogController = controller;
    }

    public @LocationAccuracy int getInitialGeolocationAccuracySelection() {
        assert mNativeDelegatePtr != null;
        return PermissionDialogDelegateJni.get()
                .getInitialGeolocationAccuracySelection(mNativeDelegatePtr);
    }

    public void onGeolocationAccuracySelected(@LocationAccuracy int locationAccuracy) {
        assert mNativeDelegatePtr != null;
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
    static PermissionDialogDelegate create(
            JniRawPtr<NativePermissionDialogDelegate> nativeDelegatePtr,
            WindowAndroid window,
            int[] contentSettingsTypes,
            int iconId,
            @JniType("std::u16string") String message,
            int[] boldedRanges,
            @JniType("std::u16string") String positiveButtonText,
            @JniType("std::u16string") String negativeButtonText,
            @JniType("std::u16string") String positiveEphemeralButtonText,
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
            JniRawPtr<NativePermissionDialogDelegate> nativeDelegatePtr,
            WindowAndroid window,
            int[] contentSettingsTypes,
            int iconId,
            @JniType("std::u16string") String message,
            int[] boldedRanges,
            @JniType("std::u16string") String positiveButtonText,
            @JniType("std::u16string") String negativeButtonText,
            @JniType("std::u16string") String positiveEphemeralButtonText,
            boolean showPositiveNonEphemeralAsFirstButton,
            @EmbeddedPromptVariant int variant) {
        mNativeDelegatePtr = nativeDelegatePtr;
        mWindow = window;
        mContentSettingsTypes = contentSettingsTypes;
        mDrawableId = iconId;
        mMessageText = message;
        for (int i = 0; i + 1 < boldedRanges.length; i += 2) {
            mBoldedRanges.add(new Pair<>(boldedRanges[i], boldedRanges[i + 1]));
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
            @JniType("std::u16string") String message,
            int[] boldedRanges,
            @JniType("std::u16string") String positiveButtonText,
            @JniType("std::u16string") String negativeButtonText,
            @JniType("std::u16string") String positiveEphemeralButtonText,
            boolean showPositiveNonEphemeralAsFirstButton,
            @EmbeddedPromptVariant int variant) {
        mContentSettingsTypes = contentSettingsTypes;
        mMessageText = message;
        mDrawableId = iconId;
        mBoldedRanges.clear();
        for (int i = 0; i + 1 < boldedRanges.length; i += 2) {
            mBoldedRanges.add(new Pair<>(boldedRanges[i], boldedRanges[i + 1]));
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
        void accept(JniPtr<NativePermissionDialogDelegate> self);

        void acceptThisTime(JniPtr<NativePermissionDialogDelegate> self);

        void acknowledge(JniPtr<NativePermissionDialogDelegate> self);

        void deny(JniPtr<NativePermissionDialogDelegate> self);

        void dismissed(
                JniPtr<NativePermissionDialogDelegate> self, @DismissalType int dismissalType);

        void resumed(JniPtr<NativePermissionDialogDelegate> self);

        void destroy(JniPtr<NativePermissionDialogDelegate> self);

        void systemPermissionResolved(JniPtr<NativePermissionDialogDelegate> self, boolean accept);

        void systemSettingsShown(JniPtr<NativePermissionDialogDelegate> self);

        int getRequestTypeEnumSize();

        void onGeolocationAccuracySelected(
                JniPtr<NativePermissionDialogDelegate> self,
                @LocationAccuracy int locationAccuracy);

        @LocationAccuracy
        int getInitialGeolocationAccuracySelection(JniPtr<NativePermissionDialogDelegate> self);

        int getGeolocationPromptType(JniPtr<NativePermissionDialogDelegate> self);
    }
}
