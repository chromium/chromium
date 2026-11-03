// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.res.Resources;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.IconProto.Icon;
import org.chromium.components.omnibox.IconResourceIdsProto.IconResourceIds;
import org.chromium.components.omnibox.OmniboxFeatures;

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
            // TODO(crbug.com/568855804): Clean up after a variation is chosen. The proto has no
            // dedicated folder icon ID.
            case IconResourceIds.ATTACH_FILE_VALUE ->
                    OmniboxFeatures.shouldShowBottomSheetPopup()
                                    && OmniboxFeatures.hasFuseboxPinnedSection()
                            ? R.drawable.ic_folder_24dp
                            : R.drawable.ic_attach_file_24dp;
            case IconResourceIds.ADD_PHOTO_ALTERNATE_VALUE -> R.drawable.add_photo_alternate_24dp;
            case IconResourceIds.LENS_CAMERA_VALUE -> R.drawable.photo_camera_24dp;
            case IconResourceIds.DRIVE_VALUE -> R.drawable.gs_drive_vd_theme_24;
            default -> Resources.ID_NULL;
        };
    }

    /**
     * Returns the client drawable mapped from {@code icon}, or {@code defaultIconRes} if {@code
     * icon} is null, has no icon id, or has no matching client drawable.
     */
    static @DrawableRes int resolveIconRes(@Nullable Icon icon, @DrawableRes int defaultIconRes) {
        if (icon == null || !icon.hasIconId()) {
            return defaultIconRes;
        }
        @DrawableRes int iconRes = getResIdForIconId(icon.getIconIdValue());
        return iconRes != Resources.ID_NULL ? iconRes : defaultIconRes;
    }
}
