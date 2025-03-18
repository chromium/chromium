// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.visited_url_ranking.url_grouping;

import org.chromium.build.annotations.NullMarked;

/**
 * GroupSuggestionsService is the core class for managing group suggestions. It represents a native
 * GroupSuggestionsService object in Java.
 */
@NullMarked
public interface GroupSuggestionsService {

    /** Delegate class to show the suggestions in UI. */
    interface Delegate {
        /** Gets called when backend has a suggestion ready to show. */
        // TODO(yuezhanggg): Add suggestion type in Java.
        default void showSuggestion() {}

        /** Gets called when backend has a dump state ready for feedback. */
        default void onDumpStateForFeedback(String dumpState) {}
    }

    /** Gets called when a tab is added. */
    void didAddTab(int tabId, int type);

    /** Registers a delegate to receive backend suggestions. */
    void registerDelegate(Delegate delegate, int windowId);

    /** Unregisters a delegate to receive backend suggestions. */
    void unregisterDelegate(Delegate delegate);
}
