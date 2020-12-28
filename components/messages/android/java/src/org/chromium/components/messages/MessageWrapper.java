// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

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
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION, this::handleActionClick)
                        .with(MessageBannerProperties.ON_SECONDARY_ACTION,
                                this::handleSecondaryActionClick)
                        .with(MessageBannerProperties.ON_DISMISSED, this::handleMessageDismissed)
                        .build();
    }

    PropertyModel getMessageProperties() {
        return mMessageProperties;
    }

    @CalledByNative
    String getTitle() {
        return mMessageProperties.get(MessageBannerProperties.TITLE);
    }

    @CalledByNative
    void setTitle(String title) {
        mMessageProperties.set(MessageBannerProperties.TITLE, title);
    }

    @CalledByNative
    String getDescription() {
        return mMessageProperties.get(MessageBannerProperties.DESCRIPTION);
    }

    @CalledByNative
    void setDescription(String description) {
        mMessageProperties.set(MessageBannerProperties.DESCRIPTION, description);
    }

    @CalledByNative
    String getPrimaryButtonText() {
        return mMessageProperties.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT);
    }

    @CalledByNative
    void setPrimaryButtonText(String primaryButtonText) {
        mMessageProperties.set(MessageBannerProperties.PRIMARY_BUTTON_TEXT, primaryButtonText);
    }

    @CalledByNative
    String getSecondaryActionText() {
        return mMessageProperties.get(MessageBannerProperties.SECONDARY_ACTION_TEXT);
    }

    @CalledByNative
    void setSecondaryActionText(String secondaryActionText) {
        mMessageProperties.set(MessageBannerProperties.SECONDARY_ACTION_TEXT, secondaryActionText);
    }

    @CalledByNative
    @DrawableRes
    int getIconResourceId() {
        return mMessageProperties.get(MessageBannerProperties.ICON_RESOURCE_ID);
    }

    @CalledByNative
    void setIconResourceId(@DrawableRes int resourceId) {
        mMessageProperties.set(MessageBannerProperties.ICON_RESOURCE_ID, resourceId);
    }

    @CalledByNative
    @DrawableRes
    int getSecondaryIconResourceId() {
        return mMessageProperties.get(MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID);
    }

    @CalledByNative
    void setSecondaryIconResourceId(@DrawableRes int resourceId) {
        mMessageProperties.set(MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID, resourceId);
    }

    @CalledByNative
    void clearNativePtr() {
        mNativeMessageWrapper = 0;
    }

    private void handleActionClick() {
        if (mNativeMessageWrapper == 0) return;
        MessageWrapperJni.get().handleActionClick(mNativeMessageWrapper);
    }

    private void handleSecondaryActionClick() {
        if (mNativeMessageWrapper == 0) return;
        MessageWrapperJni.get().handleSecondaryActionClick(mNativeMessageWrapper);
    }

    private void handleMessageDismissed() {
        // mNativeMessageWrapper can be null if the message was dismissed from native API.
        // In this case dismiss callback should have already been called.
        if (mNativeMessageWrapper == 0) return;
        MessageWrapperJni.get().handleDismissCallback(mNativeMessageWrapper);
    }

    @NativeMethods
    interface Natives {
        void handleActionClick(long nativeMessageWrapper);
        void handleSecondaryActionClick(long nativeMessageWrapper);
        void handleDismissCallback(long nativeMessageWrapper);
    }
}