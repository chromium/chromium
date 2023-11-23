// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.animation.Animator;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.ui.base.WindowAndroid;

/** A factory for constructing different Messages related objects. */
public class MessagesFactory {
    /**
     * Creates an instance of ManagedMessageDispatcher.
     *
     * @param container The MessageContainer for displaying message banners.
     * @param messageTopOffset A {@link Supplier} that supplies the top offset between message's top
     * side and app's top edge.
     * @param messageMaxTranslation A {@link Supplier} that supplies the maximum translation Y value
     * the message banner can have as a result of the animations or the gestures, relative to the
     * MessageContainer. When messages are shown, they will be animated down the screen, starting at
     * the negative |messageMaxTranslation| y translation to the resting position in the
     * MessageContainer.
     * @param autodismissDurationMs The {@link MessageAutodismissDurationProvider} providing
     * autodismiss duration for message banner.
     * @param animatorStartCallback The {@link Callback} that will be used by the message to
     * delegate starting the animations to the {@link WindowAndroid}.
     * @param windowAndroid The current window Android.
     * @return The constructed ManagedMessageDispatcher.
     */
    public static ManagedMessageDispatcher createMessageDispatcher(
            MessageContainer container,
            Supplier<Integer> messageTopOffset,
            Supplier<Integer> messageMaxTranslation,
            MessageAutodismissDurationProvider autodismissDurationMs,
            Callback<Animator> animatorStartCallback,
            WindowAndroid windowAndroid) {
        return new MessageDispatcherImpl(
                container,
                messageTopOffset,
                messageMaxTranslation,
                autodismissDurationMs,
                animatorStartCallback,
                windowAndroid);
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
