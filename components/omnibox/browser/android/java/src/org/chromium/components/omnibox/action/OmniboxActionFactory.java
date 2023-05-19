// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;

/**
 * An interface for handling interactions for Omnibox Action Chips.
 */
public class OmniboxActionFactory {
    @CalledByNative
    public static @NonNull OmniboxAction buildOmniboxPedal(
            @NonNull String hint, @OmniboxPedalType int pedalId) {
        return new OmniboxPedal(hint, pedalId);
    }

    @CalledByNative
    public static @NonNull OmniboxActionInSuggest buildActionInSuggest(@NonNull String hint,
            /* EntityInfoProto.ActionInfo.ActionType */ int actionType, @NonNull String actionUri) {
        return new OmniboxActionInSuggest(hint, actionType, actionUri);
    }

    @CalledByNative
    public static @NonNull OmniboxAction buildHistoryClustersAction(
            @NonNull String hint, @NonNull String query) {
        return new HistoryClustersAction(hint, query);
    }
}
