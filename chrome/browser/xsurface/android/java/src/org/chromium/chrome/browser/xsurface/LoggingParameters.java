// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import androidx.annotation.Nullable;

// TODO(b/269234249): Decide what to do with this class. Some of these things are feed specific.
/**
 * Implemented in Chromium.
 *
 * The set of parameters necessary for logging.
 */
public interface LoggingParameters {
    // Key for ListContentManager.getContextValues().
    String KEY = "LoggingParameters";

    /** Returns the account name to be used when logging. */
    String accountName();

    /** Returns the client instance ID used for reliability logging. */
    String clientInstanceId();

    /** Returns whether this has the same parameters as `other`. */
    @Deprecated
    default boolean loggingParametersEquals(LoggingParameters other) {
        return false;
    }

    /** Whether attention / interaction logging is enabled. */
    boolean loggingEnabled();

    /** Whether view actions may be recorded. */
    boolean viewActionsEnabled();

    /** The EventID, in raw proto bytes, of the first page response, or null if not present. */
    @Nullable
    default byte[] rootEventId() {
        return null;
    }
}
