// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import android.content.res.Resources;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageWrapper;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** Glue code between C++ and Java for passing SurveyUiDelegate. */
@JNINamespace("hats")
class SurveyUiDelegateBridge implements SurveyUiDelegate {
    private final @Nullable SurveyUiDelegate mDelegate;
    private final long mNativePointer;

    /** Called from C++ to create a new SurveyUiDelegate using a message. */
    @CalledByNative
    @VisibleForTesting
    static SurveyUiDelegateBridge createFromMessage(
            long nativePointer, MessageWrapper messageWrapper, WindowAndroid windowAndroid) {
        if (windowAndroid == null || SurveyClientFactory.getInstance() == null) return null;

        var messageDispatcher = MessageDispatcherProvider.from(windowAndroid);
        if (messageDispatcher == null) return null;

        var tabModelSelector = TabModelSelectorSupplier.getValueOrNullFrom(windowAndroid);
        if (tabModelSelector == null) return null;

        populateDefaultValuesForMessageWrapper(messageWrapper, windowAndroid);
        MessageSurveyUiDelegate delegate =
                new MessageSurveyUiDelegate(
                        messageWrapper.getMessageProperties(),
                        messageDispatcher,
                        tabModelSelector,
                        SurveyClientFactory.getInstance().getCrashUploadPermissionSupplier());

        return new SurveyUiDelegateBridge(nativePointer, delegate);
    }

    /** Called from C++ to create a new SurveyUiDelegate with customized implementations. */
    @CalledByNative
    @VisibleForTesting
    static SurveyUiDelegateBridge create(long nativePointer) {
        return new SurveyUiDelegateBridge(nativePointer, null);
    }

    @VisibleForTesting
    private static void populateDefaultValuesForMessageWrapper(
            MessageWrapper input, WindowAndroid windowAndroid) {
        Resources res = windowAndroid.getContext().get().getResources();
        PropertyModel model = input.getMessageProperties();
        MessageSurveyUiDelegate.populateDefaultValuesForSurveyMessage(res, model);
    }

    private SurveyUiDelegateBridge(long nativePointer, @Nullable SurveyUiDelegate delegate) {
        mNativePointer = nativePointer;
        mDelegate = delegate;
    }

    @Override
    public void showSurveyInvitation(
            Runnable onSurveyAccepted,
            Runnable onSurveyDeclined,
            Runnable onSurveyPresentationFailed) {
        if (mDelegate != null) {
            mDelegate.showSurveyInvitation(
                    onSurveyAccepted, onSurveyDeclined, onSurveyPresentationFailed);
            return;
        }
        SurveyUiDelegateBridgeJni.get()
                .showSurveyInvitation(
                        mNativePointer,
                        onSurveyAccepted,
                        onSurveyDeclined,
                        onSurveyPresentationFailed);
    }

    @Override
    public void dismiss() {
        if (mDelegate != null) {
            mDelegate.dismiss();
            return;
        }
        SurveyUiDelegateBridgeJni.get().dismiss(mNativePointer);
    }

    @Nullable
    SurveyUiDelegate getDelegateForTesting() {
        return mDelegate;
    }

    @NativeMethods
    interface Natives {
        void showSurveyInvitation(
                long nativeSurveyUiDelegateAndroid,
                Runnable onSurveyAccepted,
                Runnable onSurveyDeclined,
                Runnable onSurveyPresentationFailed);

        void dismiss(long nativeSurveyUiDelegateAndroid);
    }
}
