// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import androidx.annotation.Nullable;

import org.chromium.base.ActivityState;
import org.chromium.components.messages.MessageScopeChange.ChangeType;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.ActivityStateObserver;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

/**
 * Observe the webContents to notify queue manager of proper scope changes of {@link
 * MessageScopeType#NAVIGATION} and {@link MessageScopeType#WEB_CONTENTS}. Observe the windowAndroid
 * to notify queue manager of proper scope changes of {@link MessageScopeType#WINDOW}.
 */
class ScopeChangeController {
    /** A delegate which can handle the scope change. */
    public interface Delegate {
        void onScopeChange(MessageScopeChange change);
    }

    interface ScopeObserver {
        void destroy();

        boolean isActive();
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
        ScopeObserver observer =
                scopeKey.scopeType == MessageScopeType.WINDOW
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

    boolean isActive(ScopeKey scopeKey) {
        if (!mObservers.containsKey(scopeKey)) return false;
        var scopeObserver = mObservers.get(scopeKey);
        return scopeObserver.isActive();
    }

    /**
     * This handles both navigation type and webContents type. Only navigation type
     * will destroy scopes on page navigation.
     */
    static class NavigationWebContentsScopeObserver extends WebContentsObserver
            implements ScopeObserver {
        private final Delegate mDelegate;
        private final ScopeKey mScopeKey;
        // TODO(crbug.com/40230391): Replace GURL with Origin.
        private GURL mLastVisitedUrl;
        private boolean mIsActive;

        public NavigationWebContentsScopeObserver(Delegate delegate, ScopeKey scopeKey) {
            super(scopeKey.webContents);
            mDelegate = delegate;
            mScopeKey = scopeKey;
            WebContents webContents = scopeKey.webContents;
            int changeType =
                    webContents != null && webContents.getVisibility() == Visibility.VISIBLE
                            ? ChangeType.ACTIVE
                            : ChangeType.INACTIVE;
            mDelegate.onScopeChange(
                    new MessageScopeChange(mScopeKey.scopeType, scopeKey, changeType));
            mIsActive = changeType == ChangeType.ACTIVE;
        }

        @Override
        public void onVisibilityChanged(@Visibility int visibility) {
            mIsActive = visibility == Visibility.VISIBLE;
            mDelegate.onScopeChange(
                    new MessageScopeChange(
                            mScopeKey.scopeType,
                            mScopeKey,
                            mIsActive ? ChangeType.ACTIVE : ChangeType.INACTIVE));
        }

        @Override
        public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigationHandle) {
            if (mScopeKey.scopeType != MessageScopeType.NAVIGATION
                    && mScopeKey.scopeType != MessageScopeType.ORIGIN) {
                return;
            }

            if (navigationHandle.isSameDocument()
                    || !navigationHandle.hasCommitted()
                    || navigationHandle.isReload()) {
                return;
            }

            if (mScopeKey.scopeType == MessageScopeType.ORIGIN) {
                if (mLastVisitedUrl == null
                        || originEquals(mLastVisitedUrl, navigationHandle.getUrl())) {
                    mLastVisitedUrl = navigationHandle.getUrl();
                    return;
                }
                mLastVisitedUrl = navigationHandle.getUrl();
            }
            destroy();
        }

        @Override
        public void destroy() {
            super.destroy();
            // #destroy will remove the observers.
            mDelegate.onScopeChange(
                    new MessageScopeChange(mScopeKey.scopeType, mScopeKey, ChangeType.DESTROY));
            mIsActive = false;
        }

        @Override
        public boolean isActive() {
            return mIsActive;
        }

        @Override
        public void onTopLevelNativeWindowChanged(@Nullable WindowAndroid windowAndroid) {
            super.onTopLevelNativeWindowChanged(windowAndroid);
            // Dismiss the message if it is moved to another window.
            // TODO(crbug.com/40764577): This is a temporary solution; remove this when
            // tab-reparent is fully supported.
            destroy();
        }

        private boolean originEquals(GURL url1, GURL url2) {
            if (url1 == null || url2 == null) return false;
            return Objects.equals(url1.getScheme(), url2.getScheme())
                    && Objects.equals(url1.getHost(), url2.getHost())
                    && Objects.equals(url1.getPort(), url2.getPort());
        }
    }

    static class WindowScopeObserver implements ScopeObserver, ActivityStateObserver {
        private final Delegate mDelegate;
        private final ScopeKey mScopeKey;
        private boolean mIsActive;

        public WindowScopeObserver(Delegate delegate, ScopeKey scopeKey) {
            mDelegate = delegate;
            mScopeKey = scopeKey;
            assert scopeKey.scopeType == MessageScopeType.WINDOW
                    : "WindowScopeObserver should only monitor window scope events.";
            WindowAndroid windowAndroid = scopeKey.windowAndroid;
            windowAndroid.addActivityStateObserver(this);
            @ChangeType
            int changeType =
                    windowAndroid.getActivityState() == ActivityState.RESUMED
                            ? ChangeType.ACTIVE
                            : ChangeType.INACTIVE;
            mDelegate.onScopeChange(
                    new MessageScopeChange(scopeKey.scopeType, scopeKey, changeType));
            mIsActive = changeType == ChangeType.ACTIVE;
        }

        @Override
        public void onActivityPaused() {
            mDelegate.onScopeChange(
                    new MessageScopeChange(mScopeKey.scopeType, mScopeKey, ChangeType.INACTIVE));
            mIsActive = false;
        }

        @Override
        public void onActivityResumed() {
            mDelegate.onScopeChange(
                    new MessageScopeChange(mScopeKey.scopeType, mScopeKey, ChangeType.ACTIVE));
            mIsActive = true;
        }

        @Override
        public void onActivityDestroyed() {
            mDelegate.onScopeChange(
                    new MessageScopeChange(mScopeKey.scopeType, mScopeKey, ChangeType.DESTROY));
            mIsActive = false;
        }

        @Override
        public void destroy() {
            mScopeKey.windowAndroid.removeActivityStateObserver(this);
        }

        @Override
        public boolean isActive() {
            return mIsActive;
        }
    }
}
