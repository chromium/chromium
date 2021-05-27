// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

/**
 * Methods to be implemented by Chrome to support Feed logging.
 */
public interface FeedLoggingDependencyProvider {
    /** Returns whether or not activity logging should be enabled. */
    default boolean isActivityLoggingEnabled() {
        return false;
    }

    /** Returns the account name of the signed-in user, or the empty string. */
    default String getAccountName() {
        return "";
    }

    /** Returns the client instance id for this chrome. */
    default String getClientInstanceId() {
        return "";
    }

    /** Returns the collection of currently active experiment ids. */
    default int[] getExperimentIds() {
        return new int[0];
    }

    /** Returns the signed-out session id */
    default String getSignedOutSessionId() {
        return "";
    }

    /**
     * Stores a view FeedAction for eventual upload. 'data' is a serialized FeedAction protobuf
     * message.
     */
    default void processViewAction(byte[] data) {}

    /**
     * Reports whether the visibility log upload was successful.
     *
     * @param success - whether the upload was successful
     */
    default void reportOnUploadVisibilityLog(boolean success) {}
}
