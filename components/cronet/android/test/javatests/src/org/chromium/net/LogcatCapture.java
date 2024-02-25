// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.chromium.base.Log;

import java.io.BufferedReader;
import java.io.Closeable;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.UUID;

/**
 * Test utility class for capturing logcat output.
 *
 * <p>This class is useful for testing code that logs to logcat.
 *
 * <p><b>Note:</b> when using this class, it is recommended to match the resulting log lines based
 * on randomly generated markers (e.g. {@link UUID#randomUUID()}), if possible. This ensures the
 * test cannot be confused by similar code running in parallel on the same device.
 */
public final class LogcatCapture implements Closeable {
    private static final String TAG = "LogcatCapture";

    private Process mLogcat;
    private final BufferedReader mLogcatOutput;

    /**
     * Starts capturing logcat.
     *
     * <p>By default, this only captures the {@code main} log and filters out all tags and levels.
     * Use {@code additionalArgs} to customize this behavior.
     *
     * @param additionalArgs Additional arguments to pass to the logcat command. This can be used
     * to select which tags and levels to capture, e.g. {@code cr_MyTag:I}. Refer to the
     * documentation of the {@code logcat} command for details.
     */
    LogcatCapture(List<String> additionalArgs) throws IOException {
        List<String> args =
                new ArrayList<String>(
                        Arrays.asList("logcat", "-s", "-b", "main", Log.normalizeTag(TAG) + ":I"));
        args.addAll(additionalArgs);
        mLogcat = new ProcessBuilder().command(args).start();
        mLogcat.getErrorStream().close();
        mLogcat.getOutputStream().close();
        mLogcatOutput = new BufferedReader(new InputStreamReader(mLogcat.getInputStream()));

        // To ensure we only return logs that have been produced after this point, log a line with
        // a marker then consume the logcat output until we find the marker. This also provides
        // useful information to a human troubleshooting the logcat output.
        String marker = UUID.randomUUID().toString();
        Log.i(TAG, "%s --- START OF LOGCAT CAPTURE --- (command: %s)", marker, args);
        while (!readLine().contains(marker)) {}
    }

    /**
     * Waits for a log line to arrive, then returns it.
     *
     * <p>Note that, contrary to the logcat command, this will only return log lines that have been
     * produced <i>after</i> the capture started. This ensures the output is not polluted by logs
     * from previous tests.
     *
     * @return The log line. Never null.
     * @throws IllegalStateException if {@link #close()} was called
     */
    String readLine() throws IOException {
        if (mLogcat == null) {
            throw new IllegalStateException("Attempting to read a closed LogcatCapture");
        }

        String line = mLogcatOutput.readLine();
        if (line == null) {
            // We've reached end of stream, which means logcat unexpectedly closed its stdout
            // (most likely an error/crash). Clean up the process.
            close(/* reachedEndOfStream= */ true); // guaranteed to throw
        }
        return line;
    }

    /** Stops the capture. */
    @Override
    public void close() {
        close(/* reachedEndOfStream= */ false);
    }

    private void close(boolean reachedEndOfStream) {
        if (mLogcat == null) return;

        try {
            // Announce the end of the capture as a courtesy to a human reading the logcat output.
            String marker = UUID.randomUUID().toString();
            Log.i(TAG, "%s --- END OF LOGCAT CAPTURE ---", marker);

            // As a self-check, make sure the end marker shows up in the capture.
            while (!reachedEndOfStream) {
                String line = mLogcatOutput.readLine();
                if (line == null) {
                    reachedEndOfStream = true;
                } else if (line.contains(marker)) {
                    break;
                }
            }

            mLogcat.destroy();
            mLogcat.waitFor();
            mLogcat = null;
            if (reachedEndOfStream) {
                throw new RuntimeException("unexpected end of stream from logcat command");
            }
        } catch (InterruptedException exception) {
            Thread.currentThread().interrupt();
            throw new RuntimeException("Interrupted while closing logcat", exception);
        } catch (IOException exception) {
            throw new RuntimeException("I/O exception while closing logcat", exception);
        }
    }
}
