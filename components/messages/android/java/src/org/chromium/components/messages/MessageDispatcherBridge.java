// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.WebContents;

/**
 * Java counterpart to MessageDispatcherBridge. Enables C++ feature code to enqueue/dismiss messages
 * with MessageDispatcher.
 */
@JNINamespace("messages")
public class MessageDispatcherBridge {
    @CalledByNative
    private static void enqueueMessage(MessageWrapper message, WebContents webContents) {
        MessageDispatcher messageDispatcher =
                MessageDispatcherProvider.from(webContents.getTopLevelNativeWindow());
        messageDispatcher.enqueueMessage(message.getMessageProperties());
    }

    @CalledByNative
    private static void dismissMessage(MessageWrapper message, WebContents webContents) {
        MessageDispatcher messageDispatcher =
                MessageDispatcherProvider.from(webContents.getTopLevelNativeWindow());
        messageDispatcher.dismissMessage(message.getMessageProperties());
    }
}