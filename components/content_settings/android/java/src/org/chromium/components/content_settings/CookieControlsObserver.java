// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_settings;

/** Interface for a class that wants to receive cookie updates from CookieControlsBridge. */
public interface CookieControlsObserver {
    /* The following two methods are used when the UserBypassUI flag is disabled. */

    /**
     * Called when the cookie blocking status for the current page changes.
     * @param status An enum indicating the cookie blocking status.
     */
    default void onCookieBlockingStatusChanged(
            @CookieControlsStatus int status, @CookieControlsEnforcement int enforcement) {}

    /**
     * Called when there is an update in the cookies that are currently being used or blocked.
     * @param allowedCookies An integer indicating the number of cookies being used.
     * @param blockedCookies An integer indicating the number of cookies being blocked.
     */
    default void onCookiesCountChanged(int allowedCookies, int blockedCookies) {}

    /* The following three methods are used when the UserBypassUI flag is enabled. */

    /**
     * Called when the cookie blocking status for the current site changes.
     *
     * @param status An enum indicating the cookie blocking status.
     * @param enforcement An enum indicating enforcement of cookie policies.
     * @param expiration Expiration of the cookie blocking exception.
     * @param blockingStatus An enum indicating the cookie blocking status for 3PCD.
     */
    default void onStatusChanged(
            @CookieControlsStatus int status,
            @CookieControlsEnforcement int enforcement,
            @CookieBlocking3pcdStatus int blockingStatus,
            long expiration) {}

    /**
     * Called when there is an update in the number of sites where cookies are used/blocked.
     * @param allowedSites An integer indicating the number of sites with cookies being used.
     * @param blockedSites An integer indicating the number of sites with cookies being blocked.
     */
    default void onSitesCountChanged(int allowedSites, int blockedSites) {}

    /**
     * Called when the breakage confidence level for the current site changes.
     * @param level An enum indicating the confidence level.
     */
    default void onBreakageConfidenceLevelChanged(
            @CookieControlsBreakageConfidenceLevel int level) {}
}
