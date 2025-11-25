// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Collection of utility methods that operates on Tab for Fusebox. */
@NullMarked
public class FuseboxTabUtils {
    /**
     * Returns the whether a tab is eligible for attaching it's web content. This does not exclude
     * tabs based on specific tab model - including incognito tab model.
     *
     * @param tab The tab to be checked.
     */
    public static boolean isTabEligibleForAttachment(@Nullable Tab tab) {
        // TODO: This also has to check the eligibility here:
        // components/optimization_guide/content/browser/page_context_eligibility.h
        return tab != null
                && tab.isInitialized()
                && !tab.isFrozen()
                && (tab.getUrl().getScheme().equals(UrlConstants.HTTP_SCHEME)
                        || tab.getUrl().getScheme().equals(UrlConstants.HTTPS_SCHEME));
    }
}
