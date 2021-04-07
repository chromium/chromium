// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.Nullable;

/**
 * Provides dependencies for xsurface at the surface level.
 *
 * Should only be called on the UI thread.
 */
public interface SurfaceScopeDependencyProvider {
    /** Returns the activity. */
    @Nullable
    default Activity getActivity() {
        return null;
    }

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

    /** Returns the reliability logging id. */
    default long getReliabilityLoggingId() {
        return 0L;
    }

    /** User-set preference for when videos are eligible to autoplay. */
    public enum AutoplayPreference {
        /** Autoplay is disabled. */
        AUTOPLAY_DISABLED,
        /** Autoplay only occurs on Wi-Fi. */
        AUTOPLAY_ON_WIFI_ONLY,
        /** Autoplay will occur on both Wi-Fi and mobile data. */
        AUTOPLAY_ON_WIFI_AND_MOBILE_DATA
    }

    /** Returns the user-set preferences for when videos are eligible to autoplay. */
    default AutoplayPreference getAutoplayPreference() {
        return AutoplayPreference.AUTOPLAY_DISABLED;
    }

    /** Events that are triggered during the video auto-play. */
    public enum AutoplayEvent {
        /**
         * Auto-play is triggered, but not started yet. This occurs when the video card becomes
         * fully visible.
         */
        AUTOPLAY_REQUESTED,
        /** The player starts to auto-play the video. */
        AUTOPLAY_STARTED,
        /**
         * Auto-play stops before reaching the end. This occurs when the video card becomes
         * partially visible or invisible.
         */
        AUTOPLAY_STOPPED,
        /** Auto-play reaches the end. */
        AUTOPLAY_ENDED,
        /** User clicks on the auto-play video. */
        AUTOPLAY_CLICKED,
    }

    /** Reports the event related to video auto-play. */
    default void reportAutoplayEvent(AutoplayEvent event) {}
}
