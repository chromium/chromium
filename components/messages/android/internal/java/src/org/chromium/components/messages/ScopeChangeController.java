// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.ActivityState;
import org.chromium.components.messages.MessageScopeChange.ChangeType;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.ActivityStateObserver;

import java.util.HashMap;
import java.util.Map;

/**
 * Observe the webContents to notify queue manager of proper scope changes of {@link
 * MessageScopeType#NAVIGATION} and {@link MessageScopeType#WEB_CONTENTS}. Observe the windowAndroid
 * to notify queue manager of proper scope changes of {@link MessageScopeType#WINDOW}.
 */
class ScopeChangeController {
    /**
     * A delegate which can handle the scope change.
     */
    public interface Delegate {
        void onScopeChange(MessageScopeChange change);
    }

    interface ScopeObserver {
        void destroy();
    }

    private final Delegate mDelegate;
    private final Map<ScopeKey, ScopeObserver> mObservers;

    public ScopeChangeController(Delegate delegate) {
        mDelegate = delegate;
        mObservers = new HashMap<>();
    }

    /**
     * Notify every time a message is enqueued to a scope whose queue was previously empty.
     * @param scopeKey The scope key of the scope which the first message is enqueued.
     */
    void firstMessageEnqueued(ScopeKey scopeKey) {
        assert !mObservers.containsKey(scopeKey) : "This scope key has already been observed.";
        ScopeObserver observer = scopeKey.scopeType == MessageScopeType.WINDOW
                ? new WindowScopeObserver(mDelegate, scopeKey)
                : new NavigationWebContentsScopeObserver(mDelegate, scopeKey);
        mObservers.put(scopeKey, observer);
    }

    /**
     * Called when all Messages for the given {@code scopeKey} have been dismissed or removed.
     * @param scopeKey The scope key of the scope which the last message is dismissed.
     */
    void lastMessageDismissed(ScopeKey scopeKey) {
        ScopeObserver observer = mObservers.remove(scopeKey);
        observer.destroy();
    }

    /**
     * This handles both navigation type and webContents type. Only navigation type
     * will destroy scopes on page navigation.
     */
    static class NavigationWebContentsScopeObserver
            extends WebContentsObserver implements ScopeObserver {
        private final Delegate mDelegate;
        private final ScopeKey mScopeKey;

        public NavigationWebContentsScopeObserver(Delegate delegate, ScopeKey scopeKey) {
            super(scopeKey.webContents);
            mDelegate = delegate;
            mScopeKey = scopeKey;
            int changeType = ChangeType.INACTIVE;
            WebContents webContents = scopeKey.webContents;
            if (webContents != null && webContents.getViewAndroidDelegate() != null
                    && webContents.getVisibility() == Visibility.VISIBLE) {
                ViewAndroidDelegate viewAndroidDelegate = webContents.getViewAndroidDelegate();
                if (viewAndroidDelegate.getContainerView() != null
                        && viewAndroidDelegate.getContainerView().getVisibility() == View.VISIBLE) {
                    changeType = ChangeType.ACTIVE;
                } else {
                    changeType = ChangeType.INACTIVE;
                }
            }
            mDelegate.onScopeChange(
                    new MessageScopeChange(mScopeKey.scopeType, scopeKey, changeType));
        }

        @Override
        public void onWebContentsFocused() {
            mDelegate.onScopeChange(
                    new MessageScopeChange(mScopeKey.scopeType, mScopeKey, ChangeType.ACTIVE));
        }

        @Override
        public void onWebContentsLostFocus() {
            mDelegate.onScopeChange(
                    new MessageScopeChange(mScopeKey.scopeType, mScopeKey, ChangeType.INACTIVE));
        }

        @Override
        public void didFinishNavigation(NavigationHandle navigationHandle) {
            if (mScopeKey.scopeType != MessageScopeType.NAVIGATION) {
                return;
            }

            if (!navigationHandle.isInPrimaryMainFrame() || navigationHandle.isSameDocument()
                    || !navigationHandle.hasCommitted() || navigationHandle.isReload()) {
                return;
            }

            destroy();
        }

        @Override
        public void destroy() {
            super.destroy();
            // #destroy will remove the observers.
            mDelegate.onScopeChange(
                    new MessageScopeChange(mScopeKey.scopeType, mScopeKey, ChangeType.DESTROY));
        }

        @Override
        public void onTopLevelNativeWindowChanged(@Nullable WindowAndroid windowAndroid) {
            super.onTopLevelNativeWindowChanged(windowAndroid);
            // Dismiss the message if it is moved to another window.
            // TODO(crbug.com/1205392): This is a temporary solution; remove this when
            // tab-reparent is fully supported.
            destroy();
        }
    }

    static class WindowScopeObserver implements ScopeObserver, ActivityStateObserver {
        private final Delegate mDelegate;
        private final ScopeKey mScopeKey;

        public WindowScopeObserver(Delegate delegate, ScopeKey scopeKey) {
            mDelegate = delegate;
            mScopeKey = scopeKey;
            assert scopeKey.scopeType
                    == MessageScopeType.WINDOW
                : "WindowScopeObserver should only monitor window scope events.";
            WindowAndroid windowAndroid = scopeKey.windowAndroid;
            windowAndroid.addActivityStateObserver(this);
            mDelegate.onScopeChange(new MessageScopeChange(scopeKey.scopeType, scopeKey,
                    windowAndroid.getActivityState() == ActivityState.RESUMED
                            ? ChangeType.ACTIVE
                            : ChangeType.INACTIVE));
        }

        @Override
        public void onActivityPaused() {
            mDelegate.onScopeChange(
                    new MessageScopeChange(mScopeKey.scopeType, mScopeKey, ChangeType.INACTIVE));
        }

        @Override
        public void onActivityResumed() {
            mDelegate.onScopeChange(
                    new MessageScopeChange(mScopeKey.scopeType, mScopeKey, ChangeType.ACTIVE));
        }

        @Override
        public void onActivityDestroyed() {
            mDelegate.onScopeChange(
                    new MessageScopeChange(mScopeKey.scopeType, mScopeKey, ChangeType.DESTROY));
        }

        @Override
        public void destroy() {
            mScopeKey.windowAndroid.removeActivityStateObserver(this);
        }
    }
}
