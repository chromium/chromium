// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface.feed;

/**
 * Implemented internally.
 *
 * This interface provides access to the list of recently-sent "flows" of reliability logging events
 * for verification in integration tests.
 */
public interface ReliabilityLoggingTestUtil {
    /**
     * Return the most recent "flows" (at most 30), or lists of logged events
     * representing a user interaction, with each flow rendered as a string.
     */
    default String getRecentFlowsForTesting() {
        return "";
    }

    /** Return the number of recent flows that would be rendered by getRecentFlowsForTesting(). */
    default int getRecentFlowsCountForTesting() {
        return 0;
    }

    /** Clear the list of recent flows. */
    default void clearRecentFlowsForTesting() {}
}
