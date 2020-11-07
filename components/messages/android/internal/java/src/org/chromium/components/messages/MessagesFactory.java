// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.chromium.base.supplier.Supplier;
import org.chromium.ui.base.WindowAndroid;

/** A factory for constructing different Messages related objects. */
public class MessagesFactory {
    /**
     * Creates an instance of ManagedMessageDispatcher.
     * @param container The MessageContainer for displaying message banners.
     * @param messageMaxTranslation A {@link Supplier} that supplies the maximum translation Y value
     *         the message banner can have as a result of the animations or the gestures, relative
     *         to the MessageContainer. When messages are shown, they will be animated down the
     *         screen, starting at the negative |messageMaxTranslation| y translation to the resting
     *         position in the MessageContainer.
     * @return The constructed ManagedMessageDispatcher.
     */
    public static ManagedMessageDispatcher createMessageDispatcher(
            MessageContainer container, Supplier<Integer> messageMaxTranslation) {
        return new MessageDispatcherImpl(container, messageMaxTranslation);
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
