// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.find_in_page;

import android.graphics.Rect;

/**
 * Java equivalent to the C++ FindNotificationDetails class
 * defined in components/find_in_page/find_notification_details.h
 */
public class FindNotificationDetails {
    /** How many matches were found. */
    public final int numberOfMatches;

    /** Where selection occurred (in renderer window coordinates). */
    public final Rect rendererSelectionRect;

    /**
     * The ordinal of the currently selected match.
     *
     * Might be -1 even with matches in rare edge cases where the active match
     * has been removed from DOM by the time the active ordinals are processed.
     * This indicates we failed to locate and highlight the active match.
     */
    public final int activeMatchOrdinal;

    /** Whether this is the last Find Result update for the request. */
    public final boolean finalUpdate;

    public FindNotificationDetails(
            int numberOfMatches,
            Rect rendererSelectionRect,
            int activeMatchOrdinal,
            boolean finalUpdate) {
        this.numberOfMatches = numberOfMatches;
        this.rendererSelectionRect = rendererSelectionRect;
        this.activeMatchOrdinal = activeMatchOrdinal;
        this.finalUpdate = finalUpdate;
    }
}
