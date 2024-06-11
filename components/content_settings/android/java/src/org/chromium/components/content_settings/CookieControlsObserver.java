// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_settings;

import org.chromium.components.content_settings.CookieControlsBridge.TrackingProtectionFeature;

import java.util.List;

/** Interface for a class that wants to receive cookie updates from CookieControlsBridge. */
public interface CookieControlsObserver {
    /**
     * Called when the cookie blocking status for the current site changes.
     *
     * @param controlsVisible Whether the cookie controls should be visible.
     * @param protectionsOn Whether cookie blocking is enabled.
     * @param enforcement An enum indicating enforcement of cookie policies.
     * @param blockingStatus An enum indicating the cookie blocking status for 3PCD.
     * @param expiration Expiration of the cookie blocking exception.
     */
    default void onStatusChanged(
            boolean controlsVisible,
            boolean protectionsOn,
            @CookieControlsEnforcement int enforcement,
            @CookieBlocking3pcdStatus int blockingStatus,
            long expiration) {}

    /**
     * Called when the tracking protection status for the current site changes.
     *
     * @param controlsVisible Whether the tracking protection controls should be visible.
     * @param protectionsOn Whether tracking protection is enabled.
     * @param expiration Expiration of the exception for this site.
     * @param features A list of tracking protection features and status info for each.
     */
    default void onTrackingProtectionStatusChanged(
            boolean controlsVisible,
            boolean protectionsOn,
            long expiration,
            List<TrackingProtectionFeature> features) {}

    /** Called when we should surface a visual indicator due to potential site breakage. */
    default void onHighlightCookieControl(boolean shouldHighlight) {}

    /**
     * Called when we should surface a visual indicator for PWA surface due to potential site
     * breakage.
     */
    default void onHighlightPwaCookieControl() {}
}
