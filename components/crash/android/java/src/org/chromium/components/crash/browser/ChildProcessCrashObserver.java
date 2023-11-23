// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.crash.browser;

import org.jni_zero.CalledByNative;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

/**
 * A Java-side bridge for notifying an observer when a child process has crashed.
 *
 * Crashpad writes a minidump for a child process in the time between the child receives a crash
 * signal and when it exits. After the child process exits, Crashpad should have completed writing
 * a minidump to its CrashReportDatabase and this class's childCrashed() is called, executing any
 * registered callback.
 *
 * It is expected that the callback will be used to handle the newly created dump by, e.g. attaching
 * a logcat and scheduling it for upload.
 */
public class ChildProcessCrashObserver {
    private static final String TAG = "ChildCrashObserver";

    /** An interface for registering a callback to be executed when a child process crashes. */
    public interface ChildCrashedCallback {
        public void childCrashed(int pid);
    }

    /**
     * The globally registered callback for responding to child process crashes, or null if no
     * callback has been registered yet.
     */
    private static ChildCrashedCallback sCallback;

    /**
     * Registers a callback for responding to child process crashes. May be called at most once, and
     * only on the UI thread.
     *
     * @param callback The callback to trigger when a child process has exited due to a crash.
     */
    public static void registerCrashCallback(ChildCrashedCallback callback) {
        ThreadUtils.assertOnUiThread();
        assert sCallback == null;
        sCallback = callback;
    }

    /** Notifies any registered observer that a child process has exited due to an apparent crash. */
    @CalledByNative
    public static void childCrashed(int pid) {
        if (sCallback == null) {
            Log.w(TAG, "Ignoring crash observed before a callback was registered...");
            return;
        }
        sCallback.childCrashed(pid);
    }
}
