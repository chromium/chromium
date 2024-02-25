// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util.date;

import android.content.Context;
import android.text.format.DateUtils;

import org.chromium.base.ContextUtils;
import org.chromium.components.browser_ui.util.R;

import java.util.Calendar;
import java.util.Date;

/** Helper methods to deal with converting dates to various strings. */
public class StringUtils {
    private static final class LazyHolder {
        private static Calendar sCalendar1 = CalendarFactory.get();
        private static Calendar sCalendar2 = CalendarFactory.get();
    }

    private StringUtils() {}

    /**
     * Converts {@code date} into a string meant to be used as a list header.
     * @param date The {@link Date} to convert.
     * @return     The {@link CharSequence} representing the header.
     */
    public static CharSequence dateToHeaderString(Date date) {
        Context context = ContextUtils.getApplicationContext();

        LazyHolder.sCalendar1.setTimeInMillis(System.currentTimeMillis());
        LazyHolder.sCalendar2.setTime(date);

        StringBuilder builder = new StringBuilder();
        if (CalendarUtils.isSameDay(LazyHolder.sCalendar1, LazyHolder.sCalendar2)) {
            builder.append(context.getString(R.string.today)).append(" - ");
        } else {
            LazyHolder.sCalendar1.add(Calendar.DATE, -1);
            if (CalendarUtils.isSameDay(LazyHolder.sCalendar1, LazyHolder.sCalendar2)) {
                builder.append(context.getString(R.string.yesterday)).append(" - ");
            }
        }

        builder.append(
                DateUtils.formatDateTime(
                        context,
                        date.getTime(),
                        DateUtils.FORMAT_ABBREV_WEEKDAY
                                | DateUtils.FORMAT_ABBREV_MONTH
                                | DateUtils.FORMAT_SHOW_YEAR));

        return builder;
    }
}
