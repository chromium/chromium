// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util.date;

import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.DoNotInline;

import java.util.Calendar;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Helper class to simplify querying for a {@link Calendar} instance. */
public final class CalendarFactory {
    // USER_BLOCKING since we eventually .get() this.
    private static final AsyncTask<Calendar> sCalendarBuilder =
            new CalendarBuilder().executeWithTaskTraits(TaskTraits.USER_BLOCKING_MAY_BLOCK);
    private static Calendar sCalendarToClone;

    private CalendarFactory() {}

    /**
     * Call this to warm up the AsyncTask.
     *
     * Since the AsyncTask is a static field, it won't be started until the static initializer runs.
     * Calling this function simply forces the static initialized to be run.
     */
    @DoNotInline
    public static void warmUp() {}

    /**
     *
     * @return A unique {@link Calendar} instance.  This version will (1) not be handed out to any
     *         other caller and (2) will be completely reset.
     */
    public static Calendar get() {
        if (sCalendarToClone == null) {
            try {
                sCalendarToClone = (Calendar) sCalendarBuilder.get(1L, TimeUnit.MILLISECONDS);
            } catch (InterruptedException | ExecutionException | TimeoutException e) {
                // We've tried our best. If AsyncTask really does not work, we give up. :(
                sCalendarToClone = Calendar.getInstance();
            }
        }
        Calendar c = (Calendar) sCalendarToClone.clone();
        c.clear();
        return c;
    }

    private static class CalendarBuilder extends BackgroundOnlyAsyncTask<Calendar> {
        @Override
        protected Calendar doInBackground() {
            return Calendar.getInstance();
        }
    }
}
