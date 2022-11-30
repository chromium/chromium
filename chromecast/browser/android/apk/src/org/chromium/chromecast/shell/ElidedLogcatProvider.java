// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;

import java.io.BufferedReader;
import java.io.IOException;

/**
 * Extracts logcat out of Android devices and elide PII sensitive info from it.
 *
 * <p>Elided information includes: Emails, IP address, MAC address, URL/domains as well as
 * Javascript console messages.
 */
abstract class ElidedLogcatProvider {
    private static final String TAG = "ElidedLogcatProv";

    protected abstract void getRawLogcat(RawLogcatCallback rawLogcatCallback);

    protected interface RawLogcatCallback {
        public void onLogsDone(BufferedReader logsFileReader);
    }

    public interface LogcatCallback {
        public void onLogsDone(String logs);
    }

    public void getElidedLogcat(LogcatCallback callback) {
        getRawLogcat((BufferedReader logsFileReader) -> {
            // Run elideLogcat in background thread because it can be very slow
            new AsyncTaskRunner(AsyncTask.THREAD_POOL_EXECUTOR).doAsync(
                    () -> elideLogcat(logsFileReader), callback::onLogsDone);
        });
    }

    @VisibleForTesting
    protected static String elideLogcat(BufferedReader logsFileReader) {
        long startTimeMillis = SystemClock.elapsedRealtime();
        StringBuilder builder = new StringBuilder();
        try (BufferedReader autoClosableBufferedReader = logsFileReader) {
            String logLn;
            while ((logLn = autoClosableBufferedReader.readLine()) != null) {
                builder.append(LogcatElision.elide(logLn + "\n"));
            }
            long elapsedMillis = SystemClock.elapsedRealtime() - startTimeMillis;
            Log.i(TAG, "elideLogcat took " + elapsedMillis + " ms");
        } catch (IOException e) {
            Log.e(TAG, "Can't read logs", e);
        } finally {
            return builder.toString();
        }
    }
}
