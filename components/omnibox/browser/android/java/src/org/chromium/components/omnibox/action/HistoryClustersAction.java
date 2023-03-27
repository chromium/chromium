// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.omnibox.action.OmniboxActionType;
import org.chromium.chrome.browser.omnibox.action.OmniboxPedalType;
import org.chromium.url.GURL;

/**
 * Omnibox action for showing the history clusters (journeys) UI. This exists as a separate class so
 * that it can expose the associated query directly.
 */
public class HistoryClustersAction extends OmniboxPedal {
    private final String mQuery;

    public HistoryClustersAction(@NonNull String hint, @NonNull String suggestionContents,
            @NonNull String accessibilitySuffix, @NonNull String accessibilityHint,
            @Nullable GURL url, @NonNull String query) {
        super(OmniboxActionType.HISTORY_CLUSTERS, OmniboxPedalType.NONE, hint, suggestionContents,
                accessibilitySuffix, accessibilityHint, url);
        mQuery = query;
    }

    public String getQuery() {
        return mQuery;
    }

    @CalledByNative
    private static HistoryClustersAction build(@NonNull String hint,
            @NonNull String suggestionContents, @NonNull String accessibilitySuffix,
            @NonNull String accessibilityHint, @Nullable GURL url, @NonNull String query) {
        return new HistoryClustersAction(
                hint, suggestionContents, accessibilitySuffix, accessibilityHint, url, query);
    }
}
