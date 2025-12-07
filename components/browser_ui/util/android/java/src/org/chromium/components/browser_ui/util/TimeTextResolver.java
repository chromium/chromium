// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import android.util.Pair;

import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import android.content.res.Resources;

import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Resolves text representing timestamps to display to the user. */
@NullMarked
public class TimeTextResolver {

    // The order is important, as each pair is checked sequentially. The first to have at least one
    // of the given duration is used.
    private static final List<Pair<ChronoUnit, Integer>> LAST_ACCESSED_CHRONO_UNIT_AND_PLURAL_RES =
            List.of(
                    new Pair<>(ChronoUnit.MONTHS, R.plurals.time_last_accessed_months),
                    new Pair<>(ChronoUnit.WEEKS, R.plurals.time_last_accessed_weeks),
                    new Pair<>(ChronoUnit.DAYS, R.plurals.time_last_accessed_days));

    /**
     * @param timestamp The timestamp in milliseconds.
     * @return Simple text representing the time since a given timestamp. Represented as one of:
     *     Today > Yesterday > x days ago > x weeks ago > x months ago.
     */
    // duration.getSeconds should be toSeconds after api 31.
    @SuppressWarnings("JavaDurationGetSecondsToToSeconds")
    public static String resolveTimeAgoText(Resources resources, long timestamp) {
        List<Pair<ChronoUnit, Integer>> selectedChronoUnitAndPluralRes =
                LAST_ACCESSED_CHRONO_UNIT_AND_PLURAL_RES;

        long nowMillis = TimeUtils.currentTimeMillis();
        int seconds = (int) TimeUnit.MILLISECONDS.toSeconds(nowMillis - timestamp);
        for (Pair<ChronoUnit, Integer> pair : selectedChronoUnitAndPluralRes) {
            int count = (int) (seconds / pair.first.getDuration().getSeconds());
            if (count >= 1) {
                return resources.getQuantityString(pair.second, count, count);
            }
        }

        return resources.getString(R.string.time_last_accessed_today);
    }
}
