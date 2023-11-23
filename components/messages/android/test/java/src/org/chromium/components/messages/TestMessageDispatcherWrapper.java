// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.chromium.base.ObserverList;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Test implementation class for message which just collect the number of calls to
 * enqueueMessage.
 */
public class TestMessageDispatcherWrapper implements ManagedMessageDispatcher {
    private final ManagedMessageDispatcher mWrappedDispatcher;
    private final ObserverList<Observer> mMessageEnqueueObservers = new ObserverList<>();

    /** Observer that's called when new message is enqueued. */
    interface Observer {
        void onMessageEnqueued();
    }

    TestMessageDispatcherWrapper(ManagedMessageDispatcher wrappedDispatcher) {
        mWrappedDispatcher = wrappedDispatcher;
    }

    ManagedMessageDispatcher getWrappedDispatcher() {
        return mWrappedDispatcher;
    }

    public void addObserver(Observer observer) {
        mMessageEnqueueObservers.addObserver(observer);
    }

    public void removeObserver(Observer observer) {
        mMessageEnqueueObservers.removeObserver(observer);
    }

    private void notifyObserversMessageEnqueued() {
        for (var obs : mMessageEnqueueObservers) {
            obs.onMessageEnqueued();
        }
    }

    @Override
    public int suspend() {
        return mWrappedDispatcher.suspend();
    }

    @Override
    public void resume(int token) {
        mWrappedDispatcher.resume(token);
    }

    @Override
    public void setDelegate(MessageQueueDelegate wrappedDispatcher) {
        mWrappedDispatcher.setDelegate(wrappedDispatcher);
    }

    @Override
    public void dismissAllMessages(int dismissReason) {
        mWrappedDispatcher.dismissAllMessages(dismissReason);
    }

    @Override
    public void enqueueMessage(
            PropertyModel messageProperties,
            WebContents webContents,
            int scopeType,
            boolean highPriority) {
        mWrappedDispatcher.enqueueMessage(messageProperties, webContents, scopeType, highPriority);
        notifyObserversMessageEnqueued();
    }

    @Override
    public void enqueueWindowScopedMessage(PropertyModel messageProperties, boolean highPriority) {
        mWrappedDispatcher.enqueueWindowScopedMessage(messageProperties, highPriority);
        notifyObserversMessageEnqueued();
    }

    @Override
    public void dismissMessage(PropertyModel messageProperties, int dismissReason) {
        mWrappedDispatcher.dismissMessage(messageProperties, dismissReason);
    }
}
