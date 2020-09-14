// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.content.Context;

import androidx.annotation.Nullable;

/**
 * Provides dependencies for xsurface at the surface level.
 *
 * Should only be called on the UI thread.
 */
public interface SurfaceScopeDependencyProvider {
    /** Returns the activity context hosting the surface. */
    @Nullable
    default Context getActivityContext() {
        return null;
    }

    /** Returns whether the activity is in darkmode or not */
    default boolean isDarkModeEnabled() {
        return false;
    }

    /** Returns whether or not activity logging is enabled for this surface */
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
}
