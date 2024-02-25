// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util.date;

import java.util.Calendar;

/** A set of utility methods meant to make interacting with a {@link Calendar} instance easier. */
public final class CalendarUtils {
    private static final class LazyHolder {
        private static Calendar sCalendar1 = CalendarFactory.get();
        private static Calendar sCalendar2 = CalendarFactory.get();
    }

    private CalendarUtils() {}

    /**
     * Helper method to return the start of the day according to the localized {@link Calendar}.
     * This instance will have no hour, minute, second, etc. associated with it.  Only year, month,
     * and day of month.
     * @param t The timestamp (in milliseconds) since epoch.
     * @return  A {@link Calendar} instance representing the beginning of the day denoted by
     *          {@code t}.
     */
    public static Calendar getStartOfDay(long t) {
        LazyHolder.sCalendar1.setTimeInMillis(t);

        int year = LazyHolder.sCalendar1.get(Calendar.YEAR);
        int month = LazyHolder.sCalendar1.get(Calendar.MONTH);
        int day = LazyHolder.sCalendar1.get(Calendar.DATE);
        LazyHolder.sCalendar1.clear();
        LazyHolder.sCalendar1.set(year, month, day, 0, 0, 0);

        return LazyHolder.sCalendar1;
    }

    /**
     * Helper method determining whether or not two timestamps occur on the same localized calendar
     * day.
     * @param t1 A timestamp (in milliseconds) since epoch.
     * @param t2 A timestamp (in milliseconds) since epoch.
     * @return   Whether or not {@code t1} and {@code t2} represent times on the same localized
     *           calendar day.
     */
    public static boolean isSameDay(long t1, long t2) {
        LazyHolder.sCalendar1.setTimeInMillis(t1);
        LazyHolder.sCalendar2.setTimeInMillis(t2);
        return isSameDay(LazyHolder.sCalendar1, LazyHolder.sCalendar2);
    }

    /** @return Whether {@code cal1} and {@code cal2} have the same localized calendar day. */
    public static boolean isSameDay(Calendar cal1, Calendar cal2) {
        return cal1.get(Calendar.DAY_OF_YEAR) == cal2.get(Calendar.DAY_OF_YEAR)
                && cal1.get(Calendar.YEAR) == cal2.get(Calendar.YEAR);
    }
}
