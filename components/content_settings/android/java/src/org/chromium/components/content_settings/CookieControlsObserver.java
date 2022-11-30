// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_settings;

/**
 * Interface for a class that wants to receive cookie updates from CookieControlsBridge.
 */
public interface CookieControlsObserver {
    /**
     * Called when the cookie blocking status for the current page changes.
     * @param status An enum indicating the cookie blocking status.
     */
    public void onCookieBlockingStatusChanged(
            @CookieControlsStatus int status, @CookieControlsEnforcement int enforcement);

    /**
     * Called when there is an update in the cookies that are currently being used or blocked.
     * @param allowedCookies An integer indicating the number of cookies being used.
     * @param blockedCookies An integer indicating the number of cookies being blocked.
     */
    public void onCookiesCountChanged(int allowedCookies, int blockedCookies);
}
