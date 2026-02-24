// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.content.Intent;
import android.graphics.Rect;

import org.chromium.base.JniOnceCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.util.List;
import java.util.function.Supplier;

/**
 * Represents an Android window containing Chrome.
 *
 * <p>In Android, a window is a <i>Task</i>. However, depending on Task API availability in Android
 * framework, the implementation of this interface may rely on {@code ChromeActivity} as a
 * workaround to track windows.
 *
 * <p>The main difference between Task-based window tracking and {@code ChromeActivity}-based window
 * tracking is the lifecycle of a {@link ChromeAndroidTask} object, as the OS can kill {@code
 * ChromeActivity} when it's in the background, but keep its Task.
 *
 * <p>Example 1:
 *
 * <ul>
 *   <li>The user opens {@code ChromeActivity}.
 *   <li>The user then opens {@code SettingsActivity}, or an {@code Activity} not owned by Chrome.
 *   <li>If the OS decides to kill {@code ChromeActivity}, which is now in the background, {@code
 *       ChromeActivity}-based window tracking will lose the window, but the Task (window) is still
 *       alive and visible to the user.
 * </ul>
 *
 * <p>Example 2:
 *
 * <ul>
 *   <li>The user opens {@code ChromeActivity}.
 *   <li>The user moves the Task to the background.
 *   <li>If the OS decides to kill {@code ChromeActivity}, but keep its Task, {@code
 *       ChromeActivity}-based window tracking will lose the window, but the Task (window) can still
 *       be seen in "Recents" and restored by the user.
 * </ul>
 */
@NullMarked
public interface ChromeAndroidTask {

    /** Contains objects whose lifecycle is in sync with an {@code Activity}. */
    final class ActivityScopedObjects {
        final ActivityWindowAndroid mActivityWindowAndroid;
        final TabModelSelector mTabModelSelector;
        final @SupportedProfileType int mSupportedProfileType;
        final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
        final @Nullable MultiInstanceManager mMultiInstanceManager;

        public ActivityScopedObjects(
                ActivityWindowAndroid activityWindowAndroid,
                TabModelSelector tabModelSelector,
                @SupportedProfileType int supportedProfileType,
                @Nullable DesktopWindowStateManager desktopWindowStateManager,
                @Nullable MultiInstanceManager multiInstanceManager) {
            mActivityWindowAndroid = activityWindowAndroid;
            mTabModelSelector = tabModelSelector;
            assert supportedProfileType != SupportedProfileType.UNSET;
            mSupportedProfileType = supportedProfileType;
            mDesktopWindowStateManager = desktopWindowStateManager;
            mMultiInstanceManager = multiInstanceManager;
        }
    }

    /**
     * Information used to create a pending {@link ChromeAndroidTask}.
     *
     * @see ChromeAndroidTaskTracker#createPendingTask
     */
    final class PendingTaskInfo {
        /**
         * Unique ID of the pending {@link ChromeAndroidTask}.
         *
         * <p>Note that this is not the same as {@link ChromeAndroidTask#getId()}. A pending ID is
         * only for when {@link ChromeAndroidTask} isn't associated with an {@code Activity}. {@link
         * ChromeAndroidTaskTracker} uses pending IDs to track pending Tasks, and the {@code
         * Activity} will be launched with the pending ID in its {@link Intent} Extra. This allows
         * {@link ChromeAndroidTaskTracker} to pair a pending Task and a live {@code Activity} and
         * turn the pending Task into a fully initialized Task.
         */
        final int mPendingTaskId;

        /** Parameters used to create the pending {@link ChromeAndroidTask}. */
        final AndroidBrowserWindowCreateParams mCreateParams;

        /**
         * Intent used to launch the root {@code Activity} for the pending {@link
         * ChromeAndroidTask}.
         */
        final Intent mIntent;

        /**
         * Callback to notify native callers when a native {@code AndroidBrowserWindow} is created
         * and fully initialized.
         *
         * <p>The type of the callback is the address of the native {@code AndroidBrowserWindow} for
         * the initial profile. On mobile, there may be multiple {@code AndroidBrowserWindow}s for
         * different profiles.
         */
        final @Nullable JniOnceCallback<Long> mTaskCreationCallbackForNative;

        PendingTaskInfo(
                int pendingTaskId,
                AndroidBrowserWindowCreateParams createParams,
                Intent intent,
                @Nullable JniOnceCallback<Long> callback) {
            mPendingTaskId = pendingTaskId;
            mCreateParams = createParams;
            mIntent = intent;
            mTaskCreationCallbackForNative = callback;
        }

        void destroy() {
            if (mTaskCreationCallbackForNative != null) {
                mTaskCreationCallbackForNative.destroy();
            }
        }
    }

    /**
     * Returns an {@link Integer} holding the the ID of this {@link ChromeAndroidTask}, which is the
     * same as defined by {@link android.app.TaskInfo#taskId}, if the {@link Integer} is non-null.
     * The {@link Integer} will be null for a {@code State.PENDING_CREATE} {@link ChromeAndroidTask}
     * that is not yet associated with a live {@code ChromeActivity}.
     */
    @Nullable Integer getId();

    /**
     * Returns {@link PendingTaskInfo} if {@link ChromeAndroidTask} is in the {@code PENDING_CREATE}
     * state, otherwise {@code null}.
     */
    @Nullable PendingTaskInfo getPendingTaskInfo();

    /**
     * Returns the browser window type of this {@link ChromeAndroidTask}.
     *
     * <p>The types are defined in the native {@code BrowserWindowInterface::Type} enum.
     */
    @BrowserWindowType
    int getBrowserWindowType();

    /**
     * Adds an instance of {@link ActivityScopedObjects}.
     *
     * <p>As a {@link ChromeAndroidTask} is meant to track an Android Task, but {@link
     * ActivityScopedObjects} is associated with a {@code ChromeActivity}, this method is needed to
     * support the difference in their lifecycles and the fact that a Task can contain multiple
     * {@code Activities}.
     *
     * <p>The most recent {@link ActivityScopedObjects} added to a Task is considered as objects for
     * the "top" {@code Activity} in the Task.
     *
     * @param activityScopedObjects The {@link ActivityScopedObjects} to be associated with this
     *     {@link ChromeAndroidTask}.
     * @see #removeActivityScopedObjects
     */
    void addActivityScopedObjects(ActivityScopedObjects activityScopedObjects);

    /**
     * Removes the {@link ActivityScopedObjects} matching the given {@link ActivityWindowAndroid}.
     *
     * <p>This method should be called when the {@link ActivityWindowAndroid} is about to be
     * destroyed.
     *
     * <p>Note that this method may not remove {@link ActivityScopedObjects} for the top {@code
     * Activity}, as an Android Task isn't an FIFO stack. For example, the system can destroy an
     * {@code Activity} in the background and keep the foreground {@code Activity}.
     *
     * @see #addActivityScopedObjects
     */
    void removeActivityScopedObjects(ActivityWindowAndroid activityWindowAndroid);

    /**
     * Returns the top {@link ActivityWindowAndroid} in this Task, or {@code null} if there is none.
     *
     * @see #addActivityScopedObjects
     * @see #removeActivityScopedObjects
     */
    @Nullable ActivityWindowAndroid getTopActivityWindowAndroid();

    /**
     * Adds a {@link ChromeAndroidTaskFeature} to this {@link ChromeAndroidTask}.
     *
     * <p>If an instance of the given {@code featureKey} hasn't been added to this Task, this method
     * will be the start of the feature's lifecycle, and {@link
     * ChromeAndroidTaskFeature#onAddedToTask} will be invoked.
     *
     * <p>If the {@code featureKey} is profile-scoped and the profile doesn't have an associated
     * browser window this method will throw an exception.
     *
     * <p>If an instance of the given {@code featureKey} is already added, this method will be a
     * no-op and {@link ChromeAndroidTaskFeature#onAddedToTask} won't be invoked.
     *
     * <p>Production code should initialize a feature inside {@code featureSupplier}'s {@link
     * Supplier#get()} implementation to avoid unnecessarily initializing the feature if it
     * shouldn't be added.
     *
     * @param featureKey The key of the feature to add.
     * @param featureSupplier {@link Supplier} that should instantiate the feature.
     */
    <T extends ChromeAndroidTaskFeature> void addFeature(
            ChromeAndroidTaskFeatureKey featureKey, Supplier<@Nullable T> featureSupplier);

    /**
     * Creates the {@link Intent} to open a new window of type {@link BrowserWindowType#NORMAL}.
     *
     * @param isIncognito Whether the new window should be in incognito mode.
     * @return The {@link Intent} as described above, or {@code null} if a new window can't be
     *     created.
     */
    @Nullable Intent createIntentForNormalBrowserWindow(boolean isIncognito);

    /**
     * Returns the address of the native {@code BrowserWindowInterface}.
     *
     * <p>If the native object hasn't been created, this method will create it before returning its
     * address.
     *
     * @param profile The profile associated with the browser window.
     */
    long getOrCreateNativeBrowserWindowPtr(Profile profile);

    /**
     * Returns an array of the all native {@code BrowserWindowInterface} addresses.
     *
     * <p>If the native object hasn't been created, this method will create it before returning its
     * address.
     */
    List<Long> getAllNativeBrowserWindowPtrs();

    /**
     * Adds an {@link AndroidBrowserWindowObserver} for future {@code AndroidBrowserWindow} events.
     *
     * @param observer The observer to add.
     */
    void addAndroidBrowserWindowObserver(AndroidBrowserWindowObserver observer);

    /**
     * Removes an {@link AndroidBrowserWindowObserver}.
     *
     * @param observer The observer to remove.
     */
    void removeAndroidBrowserWindowObserver(AndroidBrowserWindowObserver observer);

    /** Returns whether observer is registered for {@code AndroidBrowserWindow} events. */
    boolean hasAndroidBrowserWindowObserver(AndroidBrowserWindowObserver observer);

    /**
     * Destroys all objects owned by this {@link ChromeAndroidTask}, including all {@link
     * ChromeAndroidTaskFeature}s.
     *
     * @see #addFeature
     */
    void destroy();

    /** Returns whether this {@link ChromeAndroidTask} has been destroyed. */
    boolean isDestroyed();

    /**
     * Returns whether this {@link ChromeAndroidTask} is currently in the foreground and focused.
     */
    boolean isActive();

    /** Returns whether this {@link ChromeAndroidTask} is currently maximized. */
    boolean isMaximized();

    /** Returns true if the window is minimized. */
    boolean isMinimized();

    /** Returns whether this {@link ChromeAndroidTask} is currently in fullscreen mode. */
    boolean isFullscreen();

    /** Non-maximized bounds of the task even when currently maximized or minimized. */
    Rect getRestoredBoundsInDp();

    /**
     * Returns the most recent timestamp when this {@link ChromeAndroidTask} became active, i.e.,
     * when its state changed from nonexistent or inactive (minimized/unfocused), to the active
     * state (in the foreground and focused).
     *
     * <p>The timestamp is in milliseconds since boot. Returns 0 if this task has never been
     * activated.
     */
    long getLastActivatedTimeMillis();

    /** Returns current bounds of the window. */
    Rect getBoundsInDp();

    /** Shows this {@link ChromeAndroidTask} or activates it if it's already visible. */
    void show();

    /** Returns true if the window is visible. */
    boolean isVisible();

    /** Shows this {@link ChromeAndroidTask} but does not activate it. */
    void showInactive();

    /** Closes this {@link ChromeAndroidTask}. */
    void close();

    /**
     * Move this {@link ChromeAndroidTask} to the front. This will restore the window from minimized
     * state if necessary.
     */
    void activate();

    /**
     * Unfocus this {@link ChromeAndroidTask} by making the last focused task, if any, as the active
     * window.
     */
    void deactivate();

    /** Maximize this {@link ChromeAndroidTask}. */
    void maximize();

    /** Minimizes this {@link ChromeAndroidTask}. */
    void minimize();

    /**
     * Restores this {@link ChromeAndroidTask}. This positions the window to the last known position
     * and size before it was maximized, minimized or fullscreen.
     */
    void restore();

    /**
     * Sets the {@link ChromeAndroidTask}'s size and position to the specified values.
     *
     * @param boundsInDp The new bounds of the {@link ChromeAndroidTask}.
     */
    void setBoundsInDp(Rect boundsInDp);

    /** Returns all {@link ChromeAndroidTaskFeature}s for testing. */
    List<ChromeAndroidTaskFeature> getAllFeaturesForTesting();

    /** Returns the {@link ChromeAndroidTaskFeature} instance for the given class. */
    @Nullable ChromeAndroidTaskFeature getFeatureForTesting(ChromeAndroidTaskFeatureKey featureKey);

    /**
     * Returns the {@code SessionID} as returned by {@code BrowserWindowInterface::GetSessionID()}.
     */
    @Nullable Integer getSessionIdForTesting(Profile profile);
}
