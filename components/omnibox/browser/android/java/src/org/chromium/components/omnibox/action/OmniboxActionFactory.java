// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/**
 * An interface for creation of the OmniboxAction instances.
 */
public interface OmniboxActionFactory {
    /**
     * Create a new OmniboxPedal.
     *
     * @param hint the title displayed on the chip
     * @param accessibilityHint the text to be announced to the accessibility-enabled users
     * @param pedalId the specific kind of pedal to create
     * @return new instance of an OmniboxPedal
     */
    @CalledByNative
    @NonNull
    OmniboxAction buildOmniboxPedal(long instance, @NonNull String hint,
            @NonNull String accessibilityHint, @OmniboxPedalId int pedalId);

    /**
     * Create a new OmniboxActionInSuggest.
     *
     * @param hint the title displayed on the chip
     * @param accessibilityHint the text to be announced to the accessibility-enabled users
     * @param actionType the specific type of an action matching the
     *         {@link EntityInfoProto.ActionInfo.ActionType}
     * @param actionUri the corresponding action URI/URL (serialized intent)
     * @return new instance of an OmniboxActionInSuggest
     */
    @CalledByNative
    @NonNull
    OmniboxAction buildActionInSuggest(long instance, @NonNull String hint,
            @NonNull String accessibilityHint,
            /* EntityInfoProto.ActionInfo.ActionType */ int actionType, @NonNull String actionUri);

    /**
     * Create a new HistoryClustersAction.
     *
     * @param hint the title displayed on the chip
     * @param accessibilityHint the text to be announced to the accessibility-enabled users
     * @param query the user-specific query associated with History Clusters
     * @return new instance of an HistoryClustersAction
     */
    @CalledByNative
    @NonNull
    OmniboxAction buildHistoryClustersAction(long instance, @NonNull String hint,
            @NonNull String accessibilityHint, @NonNull String query);

    @NativeMethods
    public interface Natives {
        /** Pass the OmniboxActionFactory instance to C++. */
        void setFactory(OmniboxActionFactory javaFactory);
    }
}
