// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.visited_url_ranking.url_grouping;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

/** Information about a group suggestion. */
@JNINamespace("visited_url_ranking")
@NullMarked
public class GroupSuggestion {
    public final int[] tabIds;
    public final int suggestionId;
    public final int suggestionReason;
    public final @NonNull String suggestedName;
    public final @NonNull String promoHeader;
    public final @NonNull String promoContents;

    public GroupSuggestion(
            int[] tabIds,
            int suggestionId,
            int suggestionReason,
            String suggestedName,
            String promoHeader,
            String promoContents) {
        this.tabIds = tabIds;
        this.suggestionId = suggestionId;
        this.suggestionReason = suggestionReason;
        this.suggestedName = suggestedName;
        this.promoHeader = promoHeader;
        this.promoContents = promoContents;
    }

    @CalledByNative
    private static GroupSuggestion createGroupSuggestion(
            int[] tabIds,
            int suggestionId,
            int suggestionReason,
            String suggestedName,
            String promoHeader,
            String promoContents) {
        return new GroupSuggestion(
                tabIds, suggestionId, suggestionReason, suggestedName, promoHeader, promoContents);
    }
}
