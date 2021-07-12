// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

/**
 * This interface provides access to the list of recently-sent "flows" of reliability logging events
 * for verification in integration tests.
 */
public interface ReliabilityLoggingTestUtil {
    /**
     * Return the most recent "flows" (at most 30), or lists of logged events
     * representing a user interaction, rendered as a string.
     */
    default String getRecentFlowsForTesting() {
        return "";
    }

    /** Clear the list of recent flows. */
    default void clearRecentFlowsForTesting() {}
}