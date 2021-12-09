// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

/**
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
    boolean loggingParametersEquals(LoggingParameters other);
    /** Whether attention / interaction logging is enabled. */
    boolean loggingEnabled();
    /** Whether view actions may be recorded. */
    boolean viewActionsEnabled();
}
