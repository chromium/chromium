// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface.pageinsights;

import androidx.annotation.Nullable;

/**
 * Parameters necessary for logging by {@link PageInsightsSurfaceRenderer}.
 *
 * <p>Implemented in Chromium.
 */
public interface PageInsightsLoggingParameters {
    String KEY = "PageInsightsLoggingParameters";

    /** Returns the account name to be used when logging. */
    default @Nullable String accountName() {
        return null;
    }

    /**
     * Returns serialised object representing server-impressed element to which visual elements on
     * the client should be parented for logging purposes.
     */
    default @Nullable byte[] parentData() {
        return null;
    }
}
