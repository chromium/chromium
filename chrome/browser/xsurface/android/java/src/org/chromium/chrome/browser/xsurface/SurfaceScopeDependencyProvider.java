// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;

import androidx.annotation.Nullable;

/**
 * Implemented in Chromium.
 *
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

    // Warning:
    // Code below this comment is Feed-specific, and is moving to FeedSurfaceScopeDependencyProvider

    /** User-set preference for when videos are eligible to autoplay. */
    @Deprecated
    public enum AutoplayPreference {
        /** Autoplay is disabled. */
        AUTOPLAY_DISABLED,
        /** Autoplay only occurs on Wi-Fi. */
        AUTOPLAY_ON_WIFI_ONLY,
        /** Autoplay will occur on both Wi-Fi and mobile data. */
        AUTOPLAY_ON_WIFI_AND_MOBILE_DATA
    }

    /** Returns the user-set preferences for when videos are eligible to autoplay. */
    @Deprecated
    default AutoplayPreference getAutoplayPreference() {
        return AutoplayPreference.AUTOPLAY_DISABLED;
    }

    /** Events that are triggered during the video playing. */
    @Deprecated
    public @interface VideoPlayEvent {
        // Events applying muted autoplay only.

        /**
         * Auto-play stops before reaching the end. This occurs when the video card becomes
         * partially visible or invisible.
         */
        int AUTOPLAY_STOPPED = 0;
        /** Auto-play reaches the end. */
        int AUTOPLAY_ENDED = 1;
        /** User clicks on the auto-play video. */
        int AUTOPLAY_CLICKED = 2;

        // Events applying to both muted autoplay and regular play.

        /** The player starts to play the video. */
        int PLAY_REQUESTED = 3;
        int PLAY_STARTED = 4;
        int PLAY_ERROR = 5;
        int NUM_ENTRIES = 6;
    }

    /** Errors occurred during the video player initialization. */
    @Deprecated
    public @interface VideoInitializationError {
        int CLIENT_LIBRARY_UPDATE_REQUIRED = 0;
        int DEVELOPER_KEY_INVALID = 1;
        int ERROR_CONNECTING_TO_SERVICE = 2;
        int INTERNAL_ERROR = 3;
        int INVALID_APPLICATION_SIGNATURE = 4;
        int NETWORK_ERROR = 5;
        int SERVICE_DISABLED = 6;
        int SERVICE_INVALID = 7;
        int SERVICE_MISSING = 8;
        int SERVICE_VERSION_UPDATE_REQUIRED = 9;
        int UNKNOWN_ERROR = 10;
        int NUM_ENTRIES = 11;
    }

    /** Errors occurred during the video playing. */
    @Deprecated
    public @interface VideoPlayError {
        int NOT_PLAYABLE = 0;
        int UNAUTHORIZED_OVERLAY = 1;
        int INTERNAL_ERROR = 2;
        int UNKNOWN_ERROR = 3;
        int AUTOPLAY_DISABLED = 4;
        int UNEXPECTED_SERVICE_DISCONNECTION = 5;
        int NOT_PLAYABLE_MUTED = 6;
        int NETWORK_ERROR = 7;
        int NUM_ENTRIES = 8;
    }

    /**
     * Reports the event related to video playing.
     *
     * @param isMutedAutoplay Whether the video is currently autoplaying muted.
     * @param event The event to report.
     */
    @Deprecated
    default void reportVideoPlayEvent(boolean isMutedAutoplay, @VideoPlayEvent int event) {}

    /**
     * Reports the error related to video player initialization.
     *
     * @param isMutedAutoplay Whether the video is currently autoplaying muted.
     * @param error The error to report.
     */
    @Deprecated
    default void reportVideoInitializationError(
            boolean isMutedAutoplay, @VideoInitializationError int error) {}

    /**
     * Reports the error related to video playing.
     *
     * @param isMutedAutoplay Whether the video is currently autoplaying muted.
     * @param error The error to report.
     */
    @Deprecated
    default void reportVideoPlayError(boolean isMutedAutoplay, @VideoPlayError int error) {}

    /**
     * Returns the bounds of the toolbar in global (root) coordinates.
     */
    @Deprecated
    default Rect getToolbarGlobalVisibleRect() {
        return new Rect();
    }

    /**
     * Adds a header offset observer to the surface this scope is associated with.
     *
     * @param observer The observer to add.
     * @Return a reference to be used when removing the observer, or null if not successful.
     */
    @Deprecated
    default void addHeaderOffsetObserver(SurfaceHeaderOffsetObserver observer) {}

    /**
     * Removes a header offset observer.
     *
     * @param observer An Object returned by |addHeaderOffsetObserver|.
     */
    @Deprecated
    default void removeHeaderOffsetObserver(SurfaceHeaderOffsetObserver observer) {}
}
