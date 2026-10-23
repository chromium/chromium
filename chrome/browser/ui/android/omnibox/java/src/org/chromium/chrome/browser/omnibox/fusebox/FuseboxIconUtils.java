// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.res.Resources;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.IconResourceIdsProto.IconResourceIds;

/** Utility for mapping icon ids served in Fusebox config protos to local drawables. */
@NullMarked
public final class FuseboxIconUtils {
    private FuseboxIconUtils() {}

    /**
     * Maps an {@link IconResourceIds} value found in generated protos to a local resource backed
     * drawable id.
     *
     * @param iconId The {@link IconResourceIds} value, typically from {@code
     *     Icon#getIconIdValue()}.
     * @return The matching drawable resource id, or {@link Resources#ID_NULL} if there is no client
     *     drawable for the given icon id.
     */
    public static @DrawableRes int getResIdForIconId(int iconId) {
        return switch (iconId) {
            case IconResourceIds.GLOBE_VALUE -> R.drawable.ic_globe_24dp;
            case IconResourceIds.SEARCH_LOUPE_WITH_SPARKLE_VALUE ->
                    R.drawable.search_spark_black_24dp;
            case IconResourceIds.IMAGE_CREATE_VALUE -> R.drawable.image_create_24dp;
            case IconResourceIds.BANANA_VALUE -> R.drawable.create_image_24dp;
            case IconResourceIds.TRAVEL_EXPLORE_VALUE -> R.drawable.travel_explore_24dp;
            case IconResourceIds.DRAFT_SPARK_VALUE -> R.drawable.draft_spark_24dp;
            case IconResourceIds.AUTORENEW_VALUE -> R.drawable.autorenew_24dp;
            case IconResourceIds.TIMER_VALUE -> R.drawable.ic_timer;
            case IconResourceIds.BOLT_VALUE -> R.drawable.bolt_24dp;
            case IconResourceIds.TASK_SPARK_VALUE -> R.drawable.task_spark_24dp;
            case IconResourceIds.ACUTE_VALUE -> R.drawable.acute_24dp;
            case IconResourceIds.ADD_PHOTO_ALTERNATE_VALUE -> R.drawable.add_photo_alternate_24dp;
            default -> Resources.ID_NULL;
        };
    }
}
