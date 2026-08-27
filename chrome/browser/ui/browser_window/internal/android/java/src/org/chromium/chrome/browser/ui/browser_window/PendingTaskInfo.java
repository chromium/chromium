// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.content.Intent;

import org.chromium.base.JniOnceCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Information used to create a pending {@link ChromeAndroidTask}.
 *
 * @see ChromeAndroidTaskTrackerImpl#createPendingTask
 */
@NullMarked
final class PendingTaskInfo {
    /**
     * Unique ID of the pending {@link ChromeAndroidTask}.
     *
     * <p>Note that this is not the same as {@link ChromeAndroidTask#getId()}. A pending ID is only
     * for when {@link ChromeAndroidTask} isn't associated with an {@code Activity}. {@link
     * ChromeAndroidTaskTracker} uses pending IDs to track pending Tasks, and the {@code Activity}
     * will be launched with the pending ID in its {@link Intent} Extra. This allows {@link
     * ChromeAndroidTaskTracker} to pair a pending Task and a live {@code Activity} and turn the
     * pending Task into a fully initialized Task.
     */
    final int mPendingTaskId;

    /** Parameters used to create the pending {@link ChromeAndroidTask}. */
    final AndroidBrowserWindowCreateParams mCreateParams;

    /**
     * Callback to notify native callers when a native {@code AndroidBrowserWindow} is created and
     * fully initialized.
     *
     * <p>The type of the callback is the address of the native {@code AndroidBrowserWindow} for the
     * initial profile. On mobile, there may be multiple {@code AndroidBrowserWindow}s for different
     * profiles.
     */
    final @Nullable JniOnceCallback<Long> mTaskCreationCallbackForNative;

    PendingTaskInfo(
            int pendingTaskId,
            AndroidBrowserWindowCreateParams createParams,
            @Nullable JniOnceCallback<Long> callback) {
        mPendingTaskId = pendingTaskId;
        mCreateParams = createParams;
        mTaskCreationCallbackForNative = callback;
    }

    void destroy() {
        if (mTaskCreationCallbackForNative != null) {
            mTaskCreationCallbackForNative.destroy();
        }
    }
}
