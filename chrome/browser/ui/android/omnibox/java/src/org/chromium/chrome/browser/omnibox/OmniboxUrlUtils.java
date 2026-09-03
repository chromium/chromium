// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

/** Utility methods for evaluating URLs within the Omnibox and LocationBar components. */
@NullMarked
public final class OmniboxUrlUtils {
    private OmniboxUrlUtils() {}

    /**
     * Returns whether the supplied URL represents the New Tab Page or a transient empty state.
     *
     * <p>Edge cases handled:
     *
     * <ul>
     *   <li>{@code null}: Not an NTP URL.
     *   <li>Empty or invalid: Transient state occurring on newly created tabs or windows before the
     *       initial NTP navigation commits. Treating this as an NTP candidate ensures the Default
     *       Search Engine (DSE) logo is displayed instead of the globe icon and allows early cursor
     *       focus without waiting for the navigation to commit (see crbug.com/553118979).
     *   <li>NTP URLs: Standard committed New Tab Page URLs (e.g. {@code chrome://newtab} or {@code
     *       chrome-native://newtab}).
     * </ul>
     *
     * @param url The URL to inspect.
     * @return True if the URL represents the NTP or an empty transient state.
     * @see <a href="https://crbug.com/553118979">crbug.com/553118979</a>
     */
    public static boolean isNtpUrl(@Nullable GURL url) {
        return url != null && (GURL.isEmptyOrInvalid(url) || UrlUtilities.isNtpUrl(url));
    }
}
