// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.visited_url_ranking.url_grouping;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

/**
 * GroupSuggestionsService is the core class for managing group suggestions. It represents a native
 * GroupSuggestionsService object in Java.
 */
@NullMarked
public interface GroupSuggestionsService {

    /** Delegate class to show the suggestions in UI. */
    interface Delegate {
        /** Gets called when backend has a suggestion ready to show. */
        default void showSuggestion(
                GroupSuggestions groupSuggestions, Callback<UserResponseMetadata> callback) {}

        /** Gets called when backend has a dump state ready for feedback. */
        default void onDumpStateForFeedback(String dumpState) {}
    }

    /** Gets called when a tab is added. */
    void didAddTab(int tabId, int tabLaunchType);

    /** Gets called when a tab is selected. */
    void didSelectTab(int tabId, GURL url, int tabSelectionType, int lastTabId);

    /** Gets called when a tab will be closed. */
    void willCloseTab(int tabId);

    /** Gets called when a tab closure will be undone. */
    void tabClosureUndone(int tabId);

    /** Gets called when a tab closure is committed. */
    void tabClosureCommitted(int tabId);

    /** Gets called when user enters tab switcher. */
    void didEnterTabSwitcher();

    /** Gets called when user-initiated page navigation finishes on any candidate tab. */
    void onDidFinishNavigation(int tabId, int transitionType);

    /** Registers a delegate to receive backend suggestions. */
    void registerDelegate(Delegate delegate, int windowId);

    /** Unregisters a delegate to receive backend suggestions. */
    void unregisterDelegate(Delegate delegate);
}
