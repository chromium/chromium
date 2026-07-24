// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.util.ArrayMap;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;

import java.util.Locale;
import java.util.Map;
import java.util.Set;

/** POD-type that holds the top margin for each AnchorContainer's layout. */
@NullMarked
/* package-private */ final class AnchorContainerTopMargins {
    private final Map<@AnchorSide Integer, Integer> mTopMargins;

    AnchorContainerTopMargins(Map<@AnchorSide Integer, Integer> topMargins) {
        mTopMargins = topMargins;
    }

    Set<Map.Entry<@AnchorSide Integer, Integer>> entrySet() {
        return mTopMargins.entrySet();
    }

    @Nullable Integer get(@AnchorSide int anchorSide) {
        return mTopMargins.get(anchorSide);
    }

    /** Returns true if there is no top margin defined for {@link AnchorSide}. */
    boolean isEmpty() {
        return mTopMargins.isEmpty();
    }

    /**
     * Calculates the difference between this {@link AnchorContainerTopMargins} and the given {@link
     * AnchorContainerTopMargins}.
     *
     * <p>For each {@link AnchorSide}, if the specs are different, the returned {@link
     * AnchorContainerTopMargins} retains the spec of this {@link AnchorContainerTopMargins}.
     *
     * <p>The returned {@link AnchorContainerTopMargins} is useful for only updating the parts in
     * the UI that are changed.
     *
     * @param other The other {@link AnchorContainerTopMargins} to compare against.
     * @return A {@link AnchorContainerTopMargins} representing the diff.
     */
    AnchorContainerTopMargins diffAgainst(AnchorContainerTopMargins other) {
        Map<@AnchorSide Integer, Integer> topMarginDiffs = new ArrayMap<>();
        for (var entry : mTopMargins.entrySet()) {
            @AnchorSide int anchorSide = entry.getKey();
            @Px int thisMargin = entry.getValue();
            @Nullable Integer otherMargin = other.get(anchorSide);
            if (otherMargin == null || thisMargin != otherMargin) {
                topMarginDiffs.put(anchorSide, thisMargin);
            }
        }
        return new AnchorContainerTopMargins(topMarginDiffs);
    }

    @Override
    public String toString() {
        return String.format(
                Locale.ENGLISH,
                "[LeftTopMargin: %s, RightTopMargin: %s]",
                get(AnchorSide.LEFT),
                get(AnchorSide.RIGHT));
    }
}
