// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.chromium.components.messages.MessageScopeChange.ChangeType;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.PageTransition;

import java.util.HashMap;
import java.util.Map;

/**
 * Observe the webContents and notify queue manager of proper scope changes.
 */
class ScopeChangeController {
    /**
     * A delegate which can handle the scope change.
     */
    public interface Delegate {
        void onScopeChange(MessageScopeChange change);
    }

    private final Delegate mDelegate;
    private final Map<ScopeKey, WebContentsObserver> mObservers;

    public ScopeChangeController(Delegate delegate) {
        mDelegate = delegate;
        mObservers = new HashMap<>();
    }

    /**
     * Notify every time a message is enqueued to a scope whose queue was previously empty.
     * @param scopeKey The scope key of the scope which the first message is enqueued.
     */
    void firstMessageEnqueued(ScopeKey scopeKey) {
        WebContentsObserver observer = createObserver(scopeKey);
        assert !mObservers.containsKey(scopeKey) : "This scope key has already been observed.";
        mObservers.put(scopeKey, observer);
        mDelegate.onScopeChange(new MessageScopeChange(scopeKey.scopeType, scopeKey,
                scopeKey.webContents.getVisibility() == Visibility.VISIBLE ? ChangeType.ACTIVE
                                                                           : ChangeType.INACTIVE));
    }

    /**
     * Called when all Messages for the given {@code scopeKey} have been dismissed or removed.
     * @param scopeKey The scope key of the scope which the last message is dismissed.
     */
    void lastMessageDismissed(ScopeKey scopeKey) {
        WebContentsObserver observer = mObservers.remove(scopeKey);
        observer.destroy();
    }

    private WebContentsObserver createObserver(ScopeKey scopeKey) {
        WebContents webContents = scopeKey.webContents;
        @MessageScopeType
        int scopeType = scopeKey.scopeType;
        return new WebContentsObserver(webContents) {
            @Override
            public void wasShown() {
                super.wasShown();
                mDelegate.onScopeChange(
                        new MessageScopeChange(scopeType, scopeKey, ChangeType.ACTIVE));
            }

            @Override
            public void wasHidden() {
                super.wasHidden();
                mDelegate.onScopeChange(
                        new MessageScopeChange(scopeKey.scopeType, scopeKey, ChangeType.INACTIVE));
            }

            @Override
            public void navigationEntryCommitted(LoadCommittedDetails details) {
                if (scopeKey.scopeType != MessageScopeType.NAVIGATION) {
                    return;
                }
                if (!details.isMainFrame() || details.isSameDocument()
                        || details.didReplaceEntry()) {
                    return;
                }
                super.navigationEntryCommitted(details);
                NavigationController controller = scopeKey.webContents.getNavigationController();
                NavigationEntry entry =
                        controller.getEntryAtIndex(controller.getLastCommittedEntryIndex());

                int transition = entry.getTransition();
                if ((transition & PageTransition.RELOAD) != PageTransition.RELOAD
                        && (transition & PageTransition.IS_REDIRECT_MASK) == 0) {
                    destroy();
                }
            }

            @Override
            public void destroy() {
                super.destroy();
                // #destroy will remove the observers.
                mDelegate.onScopeChange(
                        new MessageScopeChange(scopeType, scopeKey, ChangeType.DESTROY));
            }
        };
    }
}
