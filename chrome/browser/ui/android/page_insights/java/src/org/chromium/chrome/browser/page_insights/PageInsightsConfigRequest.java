// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHandle;

import java.util.Objects;

/** A request for a {@link PageInsightsConfig}. */
public class PageInsightsConfigRequest {
    private final @Nullable NavigationHandle mNavigationHandle;
    private final @Nullable NavigationEntry mNavigationEntry;
    private final boolean mHasPageLoadBeenStartedSinceCreation;

    public PageInsightsConfigRequest(
            @Nullable NavigationHandle navigationHandle,
            @Nullable NavigationEntry navigationEntry,
            boolean hasPageLoadBeenStartedSinceCreation) {
        mNavigationHandle = navigationHandle;
        mNavigationEntry = navigationEntry;
        mHasPageLoadBeenStartedSinceCreation = hasPageLoadBeenStartedSinceCreation;
    }

    /** Returns the {@link NavigationHandle} for the current URL, if available. */
    public @Nullable NavigationHandle getNavigationHandle() {
        return mNavigationHandle;
    }

    /** Returns the {@link NavigationEntry} for the current URL, if available. */
    public @Nullable NavigationEntry getNavigationEntry() {
        return mNavigationEntry;
    }

    /**
     * Returns true if since the Page Insights component was created there has been at least one
     * page load started.
     */
    public boolean hasPageLoadBeenStartedSinceCreation() {
        return mHasPageLoadBeenStartedSinceCreation;
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof PageInsightsConfigRequest request)) {
            return false;
        }
        return Objects.equals(mNavigationHandle, request.getNavigationHandle())
                && Objects.equals(mNavigationEntry, request.getNavigationEntry())
                && mHasPageLoadBeenStartedSinceCreation
                        == request.hasPageLoadBeenStartedSinceCreation();
    }
}
