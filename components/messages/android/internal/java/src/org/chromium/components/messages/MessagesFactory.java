// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.chromium.ui.base.WindowAndroid;

/** A factory for constructing different Messages related objects. */
public class MessagesFactory {
    /**
     * Creates an instance of ManagedMessageDispatcher.
     * @param container The MessageContainer for displaying message banners.
     * @return The constructed ManagedMessageDispatcher.
     */
    public static ManagedMessageDispatcher createMessageDispatcher(MessageContainer container) {
        return new MessageDispatcherImpl(container);
    }

    /**
     * Attaches MessageDispatcher as UnownedUserData to WindowAndroid making it accessible to
     * components outside of chrome/android.
     * @param windowAndroid The WindowAndroid to attach ManagedMessageDispatcher to.
     * @param messageDispatcher The MessageDispatcher to attach.
     */
    public static void attachMessageDispatcher(
            WindowAndroid windowAndroid, ManagedMessageDispatcher messageDispatcher) {
        MessageDispatcherProvider.attach(windowAndroid, messageDispatcher);
    }

    /**
     * Detaches MessageDispatcher from WindowAndroid.
     * @param messageDispatcher The MessageDispatcher to detach from WindowAndroid.
     */
    public static void detachMessageDispatcher(ManagedMessageDispatcher messageDispatcher) {
        MessageDispatcherProvider.detach(messageDispatcher);
    }
}
