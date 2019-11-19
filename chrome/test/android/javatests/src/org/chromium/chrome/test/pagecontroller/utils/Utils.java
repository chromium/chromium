// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import android.os.SystemClock;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Log;

import java.util.Collections;
import java.util.List;

/**
 * Utility methods for Pagecontroller.
 */

public final class Utils {
    private static final String TAG = "PageController Utils";

    /**
     * Calculates the time interval from previousTime to now.
     * @param previousTime  The previousTime, as returned from a previous call to currentTime().
     * @return  The number of milliseconds that has elapsed since previousTime.
     */
    public static long elapsedTime(long previousTime) {
        return currentTime() - previousTime;
    }

    /**
     * Gets the currentTime, to be used with elapsedTime to calculate time intervals.
     * @return  The currentTime in milliseconds.
     */
    public static long currentTime() {
        return SystemClock.uptimeMillis();
    }

    /**
     * Sleeps for ms, logs but does not throw exceptions.
     * @param ms Milliseconds to sleep for.
     */
    public static void sleep(long ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException e) {
            Log.e(TAG, "Sleep interrupted", e);
        }
    }

    /**
     * Convert singleton to list.
     * @param t Possibly null singleton.
     * @return Empty list if t is null, else return a singleton list containing t.
     */
    public static <T> List<T> nullableIntoList(@Nullable T t) {
        return t == null ? Collections.<T>emptyList() : Collections.singletonList(t);
    }

    /**
     * Convert list and index into singleton.
     * @param list  List to be traversed.
     * @param index 0-based index into the list.
     * @return      The index-th item in the list or null if it's out of bounds.
     */
    public static @Nullable<T> T nullableGet(@NonNull List<T> list, int index) {
        return index >= list.size() ? null : list.get(index);
    }
}
