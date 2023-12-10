// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Java counterpart to MessageDispatcherBridge. Enables C++ feature code to enqueue/dismiss messages
 * with MessageDispatcher.
 */
@JNINamespace("messages")
public class MessageDispatcherBridge {
    /**
     * Return false if it fails to enqueue message, which usually happens when
     * activity is being recreated or destroyed; otherwise, return true.
     */
    @CalledByNative
    private static boolean enqueueMessage(
            MessageWrapper message,
            WebContents webContents,
            @MessageScopeType int scopeType,
            boolean highPriority) {
        MessageDispatcher messageDispatcher =
                MessageDispatcherProvider.from(webContents.getTopLevelNativeWindow());
        if (messageDispatcher == null) return false;
        messageDispatcher.enqueueMessage(
                message.getMessageProperties(), webContents, scopeType, highPriority);
        return true;
    }

    @CalledByNative
    private static boolean enqueueWindowScopedMessage(
            MessageWrapper message, WindowAndroid windowAndroid, boolean highPriority) {
        MessageDispatcher messageDispatcher = MessageDispatcherProvider.from(windowAndroid);
        if (messageDispatcher == null) return false;
        messageDispatcher.enqueueWindowScopedMessage(message.getMessageProperties(), highPriority);
        return true;
    }

    @CalledByNative
    private static void dismissMessage(
            MessageWrapper message, WindowAndroid windowAndroid, @DismissReason int dismissReason) {
        MessageDispatcher messageDispatcher = MessageDispatcherProvider.from(windowAndroid);
        if (messageDispatcher == null) return;
        messageDispatcher.dismissMessage(message.getMessageProperties(), dismissReason);
    }
}
