// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.ui.autofill.data.AuthenticatorOption;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.List;

/** JNI Bridge for {@link AuthenticatorSelectionDialog} */
@JNINamespace("autofill")
public class AuthenticatorSelectionDialogBridge implements AuthenticatorSelectionDialog.Listener {
    private final long mNativeCardUnmaskAuthenticationSelectionDialogView;
    private AuthenticatorSelectionDialog mAuthenticatorSelectionDialog;

    public AuthenticatorSelectionDialogBridge(
            long nativeAuthenticatorSelectionDialogView,
            Context context,
            ModalDialogManager modalDialogManager) {
        mNativeCardUnmaskAuthenticationSelectionDialogView = nativeAuthenticatorSelectionDialogView;
        mAuthenticatorSelectionDialog =
                new AuthenticatorSelectionDialog(context, this, modalDialogManager);
    }

    @Nullable
    @CalledByNative
    public static AuthenticatorSelectionDialogBridge create(
            long nativeAuthenticatorSelectionDialogView, WindowAndroid windowAndroid) {
        Context context = windowAndroid.getActivity().get();
        ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        if (context == null || modalDialogManager == null) {
            return null;
        }
        return new AuthenticatorSelectionDialogBridge(
                nativeAuthenticatorSelectionDialogView, context, modalDialogManager);
    }

    /**
     * Create a Java List of {@link AuthenticatorOption} and return it.
     *
     * @return List of AuthenticatorOptions.
     */
    @CalledByNative
    private static List<AuthenticatorOption> createAuthenticatorOptionList() {
        return new ArrayList<>();
    }

    /**
     * Constructs an {@link AuthenticatorOption} object and adds it to the list that was passed in.
     *
     * @param list List to add to.
     * @param title Title of {@link AuthenticatorOption}.
     * @param identifier id of {@link AuthenticatorOption}.
     * @param description Description of {@link AuthenticatorOption}.
     * @param type type of {@link CardUnmaskChallengeOptionType}. Used to determine the icon that
     *         should be shown.
     */
    @CalledByNative
    private static void createAuthenticatorOptionAndAddToList(
            List<AuthenticatorOption> list,
            String title,
            String identifier,
            String description,
            @CardUnmaskChallengeOptionType int type) {
        if (list == null) {
            return;
        }

        int iconResId = 0;
        // We need to map the icon on this side, since the ID isn't available on the C++ side.
        switch (type) {
            case CardUnmaskChallengeOptionType.SMS_OTP:
                iconResId = R.drawable.ic_outline_sms_24dp;
                break;
            case CardUnmaskChallengeOptionType.EMAIL_OTP:
                iconResId = R.drawable.ic_outline_email_24dp;
                break;
            case CardUnmaskChallengeOptionType.CVC:
                break;
            case CardUnmaskChallengeOptionType.UNKNOWN_TYPE:
                // This will never happen
                assert false : "Attempted to offer CardUnmaskChallengeOption with Unknown type";
        }
        AuthenticatorOption authenticatorOption =
                new AuthenticatorOption.Builder()
                        .setTitle(title)
                        .setIdentifier(identifier)
                        .setDescription(description)
                        .setIconResId(iconResId)
                        .setType(type)
                        .build();
        list.add(authenticatorOption);
    }

    /**
     * Shows an Authenticator Selection dialog.
     *
     * @param authenticatorOptions The authenticator options available to the user.
     */
    @CalledByNative
    public void show(List<AuthenticatorOption> authenticatorOptions) {
        mAuthenticatorSelectionDialog.show(authenticatorOptions);
    }

    /** Dismisses the Authenticator Selection Dialog. */
    @CalledByNative
    public void dismiss() {
        mAuthenticatorSelectionDialog.dismiss(DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    /**
     * Notify that the user selected an authenticator option.
     *
     * @param authenticatorOptionIdentifier the identifier of the selected option.
     */
    @Override
    public void onOptionSelected(String authenticatorOptionIdentifier) {
        AuthenticatorSelectionDialogBridgeJni.get()
                .onOptionSelected(
                        mNativeCardUnmaskAuthenticationSelectionDialogView,
                        authenticatorOptionIdentifier);
    }

    /** Notify that the dialog was dismissed. */
    @Override
    public void onDialogDismissed() {
        AuthenticatorSelectionDialogBridgeJni.get()
                .onDismissed(mNativeCardUnmaskAuthenticationSelectionDialogView);
    }

    @NativeMethods
    interface Natives {
        void onOptionSelected(
                long nativeAuthenticatorSelectionDialogViewAndroid,
                String authenticatorOptionIdentifier);

        void onDismissed(long nativeAuthenticatorSelectionDialogViewAndroid);
    }
}
