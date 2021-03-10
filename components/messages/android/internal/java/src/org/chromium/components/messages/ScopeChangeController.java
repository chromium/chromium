// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.messages.MessageScopeChange.ChangeType;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * Observe the webContents and notify queue manager of proper scope changes.
 */
class ScopeChangeController {
    private final MessageQueueManager mQueueManager;
    private final Map<Object, WebContents> mMessageToWebContentsMap;
    private final Map<WebContents, RefCountWebContentsObserver> mRefCountedWebContentsObserverMap;

    public ScopeChangeController(MessageQueueManager queueManager) {
        mQueueManager = queueManager;
        mMessageToWebContentsMap = new HashMap<>();
        mRefCountedWebContentsObserverMap = new HashMap<>();
    }

    /**
     * Observe the given webContents and notify queue manager of proper scope changes.
     * @param messageKey The Object key to differentiate the messages.
     * @param webContents The webContents to be observed.
     */
    void observe(Object messageKey, WebContents webContents) {
        // If one observer has been observing the given webContents, no need to create a new one.
        RefCountWebContentsObserver webObserver =
                mRefCountedWebContentsObserverMap.get(webContents);
        if (webObserver != null) {
            webObserver.increaseCount();
        } else {
            webObserver = createObserver(webContents);
            mQueueManager.onScopeChange(new MessageScopeChange(MessageScopeType.WEB_CONTENTS,
                    webContents,
                    webContents.getVisibility() == Visibility.VISIBLE ? ChangeType.ACTIVE
                                                                      : ChangeType.INACTIVE));
            mRefCountedWebContentsObserverMap.put(webContents, webObserver);
        }
        mMessageToWebContentsMap.put(messageKey, webContents);
    }

    void stopObservation(Object messageKey) {
        WebContents webContents = mMessageToWebContentsMap.get(messageKey);
        if (webContents == null) return;
        mMessageToWebContentsMap.remove(messageKey);
        RefCountWebContentsObserver webObserver =
                mRefCountedWebContentsObserverMap.get(webContents);
        assert webObserver != null;
        webObserver.decreaseCount();
    }

    void stopAllObservation() {
        // toArray: to avoid ConcurrentModificationException.
        for (Object messageKey : mMessageToWebContentsMap.keySet().toArray()) {
            stopObservation(messageKey);
        }
    }

    private void removeObservers(WebContents webContents) {
        mRefCountedWebContentsObserverMap.remove(webContents);
        for (Iterator<WebContents> it = mMessageToWebContentsMap.values().iterator();
                it.hasNext();) {
            if (it.next() == webContents) it.remove();
        }
    }

    private RefCountWebContentsObserver createObserver(WebContents webContents) {
        return new RefCountWebContentsObserver(
                webContents, () -> { removeObservers(webContents); }) {
            @Override
            public void wasShown() {
                super.wasShown();
                mQueueManager.onScopeChange(new MessageScopeChange(
                        MessageScopeType.WEB_CONTENTS, webContents, ChangeType.ACTIVE));
            }

            @Override
            public void wasHidden() {
                super.wasHidden();
                mQueueManager.onScopeChange(new MessageScopeChange(
                        MessageScopeType.WEB_CONTENTS, webContents, ChangeType.INACTIVE));
            }

            // TODO(crbug.com/1163290): dismiss message when page is navigated to another site.

            @Override
            public void destroy() {
                super.destroy();
                // #destroy will remove the observers.
                mQueueManager.onScopeChange(new MessageScopeChange(
                        MessageScopeType.WEB_CONTENTS, webContents, ChangeType.DESTROY));
            }
        };
    }

    @VisibleForTesting
    Map<Object, WebContents> getMessageToWebContentsMap() {
        return mMessageToWebContentsMap;
    }

    @VisibleForTesting
    Map<WebContents, RefCountWebContentsObserver> getRefCountedWebContentsObserverMap() {
        return mRefCountedWebContentsObserverMap;
    }

    /**
     * A web contents observer which can count references and call callback when all references
     * have been cleared or when destroyed. Observer will be destroyed as well when all references
     * have been cleared.
     */
    @VisibleForTesting
    static class RefCountWebContentsObserver extends WebContentsObserver {
        public final WebContents webContents;
        private int mCount;
        private final Runnable mOnEmpty;

        public RefCountWebContentsObserver(WebContents webContents, Runnable onEmpty) {
            super(webContents);
            this.webContents = webContents;
            mCount = 1;
            mOnEmpty = onEmpty;
        }

        public void increaseCount() {
            mCount++;
        }

        public void decreaseCount() {
            mCount--;
            if (mCount == 0) {
                // Call super instead of this to avoid notifying MQM about ChangeType.DESTROY.
                super.destroy();
                mOnEmpty.run();
            }
        }

        @Override
        public void destroy() {
            super.destroy();
            mOnEmpty.run();
        }
    }
}
