// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.omnibox.R;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;

/**
 * Omnibox action for showing the history clusters (journeys) UI. This exists as a separate class so
 * that it can expose the associated query directly.
 */
public class HistoryClustersAction extends OmniboxAction {
    /** The default action icon for Journeys. */
    @VisibleForTesting
    static final ChipIcon JOURNEYS_ICON =
            new ChipIcon(R.drawable.action_journeys, /*tintWithTextColor=*/true);
    /** Associated user query, guaranteed to be a non-empty string. */
    public final @NonNull String query;

    public HistoryClustersAction(long nativeInstance, @NonNull String hint,
            @NonNull String accessibilityHint, @NonNull String query) {
        super(OmniboxActionId.HISTORY_CLUSTERS, nativeInstance, hint, accessibilityHint,
                JOURNEYS_ICON);
        assert !TextUtils.isEmpty(query);
        this.query = query;
    }

    @Override
    public void execute(@NonNull OmniboxActionDelegate delegate) {
        delegate.openHistoryClustersPage(query);
    }

    /**
     * Cast supplied OmniboxAction to HistoryClustersAction.
     * Requires the supplied input to be a valid instance of an HistoryClustersAction whose
     * actionId is the HISTORY_CLUSTERS_ACTION.
     */
    public static @NonNull HistoryClustersAction from(@NonNull OmniboxAction action) {
        assert action != null;
        assert action.actionId == OmniboxActionId.HISTORY_CLUSTERS;
        assert action instanceof HistoryClustersAction;
        return (HistoryClustersAction) action;
    }
}
