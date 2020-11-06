// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.view.View;

import androidx.annotation.DrawableRes;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Java side of native MessageWrapper class that represents a message for native features.
 */
@JNINamespace("messages")
public final class MessageWrapper {
    private long mNativeMessageWrapper;
    private final PropertyModel mMessageProperties;

    /**
     * Creates an instance of MessageWrapper and links it with native MessageWrapper object.
     * @param nativeMessageWrapper Pointer to native MessageWrapper.
     * @return reference to created MessageWrapper.
     */
    @CalledByNative
    static MessageWrapper create(long nativeMessageWrapper) {
        return new MessageWrapper(nativeMessageWrapper);
    }

    private MessageWrapper(long nativeMessageWrapper) {
        mNativeMessageWrapper = nativeMessageWrapper;
        mMessageProperties =
                new PropertyModel.Builder(MessageBannerProperties.SINGLE_ACTION_MESSAGE_KEYS)
                        .with(MessageBannerProperties.PRIMARY_BUTTON_CLICK_LISTENER,
                                this::handleActionClick)
                        .with(MessageBannerProperties.ON_DISMISSED, this::handleMessageDismissed)
                        .build();
    }

    PropertyModel getMessageProperties() {
        return mMessageProperties;
    }

    @CalledByNative
    void setTitle(String title) {
        mMessageProperties.set(MessageBannerProperties.TITLE, title);
    }

    @CalledByNative
    void setDescription(String description) {
        mMessageProperties.set(MessageBannerProperties.DESCRIPTION, description);
    }

    @CalledByNative
    void setPrimaryButtonText(String primaryButtonText) {
        mMessageProperties.set(MessageBannerProperties.PRIMARY_BUTTON_TEXT, primaryButtonText);
    }

    @CalledByNative
    void setIconResourceId(@DrawableRes int resourceId) {
        mMessageProperties.set(MessageBannerProperties.ICON_RESOURCE_ID, resourceId);
    }

    @CalledByNative
    void clearNativePtr() {
        mNativeMessageWrapper = 0;
    }

    private void handleActionClick(View v) {
        if (mNativeMessageWrapper == 0) return;
        MessageWrapperJni.get().handleActionClick(mNativeMessageWrapper);
    }

    private void handleMessageDismissed() {
        MessageWrapperJni.get().handleDismissCallback(mNativeMessageWrapper);
    }

    @NativeMethods
    interface Natives {
        void handleActionClick(long nativeMessageWrapper);
        void handleDismissCallback(long nativeMessageWrapper);
    }
}