// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement;

import androidx.annotation.CheckResult;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;

/**
 * Tracker is the Java representation of a native Tracker object.
 * It is owned by the native BrowserContext.
 *
 * Tracker is the core class for the feature engagement.
 */
public interface Tracker {
    /** A handle for the display lock. While this is unreleased, no in-product help can be displayed. */
    interface DisplayLockHandle {
        /**
         * This method must be invoked when the lock should be released, and it must be invoked on
         * the main thread.
         */
        void release();
    }

    /** Must be called whenever an event happens. */
    void notifyEvent(String event);

    /**
     * This function must be called whenever the triggering condition for a specific feature
     * happens. Returns true iff the display of the in-product help must happen.
     * If {@code true} is returned, the caller *must* call {@link #dismissed(String)} when display
     * of feature enlightenment ends.
     * @param feature The name of the feature requesting in-product help.
     * @return Whether feature enlightenment should be displayed.
     */
    @CheckResult
    boolean shouldTriggerHelpUI(String feature);

    /**
     * For callers interested in showing a snooze button. For other callers, use the
     * ShouldTriggerHelpUI(..) method.
     * @param feature The name of the feature requesting in-product help.
     * @return Whether feature enlightenment should be displayed and whether snooze button should be
     *         shown.
     */
    @CheckResult
    TriggerDetails shouldTriggerHelpUIWithSnooze(String feature);

    /**
     * Invoking this is basically the same as being allowed to invoke {@link
     * #shouldTriggerHelpUI(String)} without requiring to show the in-product help. This function
     * may be called to inspect if the current state would allow the given {@code feature} to pass
     * all its conditions and display the feature enlightenment.
     *
     * NOTE: It is still required to invoke ShouldTriggerHelpUI(...) if feature enlightenment should
     * be shown.
     *
     * NOTE: It is not guaranteed that invoking {@link #shouldTriggerHelpUI(String)} after this
     * would yield the same result. The state might change in-between the calls because time has
     * passed, other events might have been triggered, and other state might have changed.
     *
     * @return Whether feature enlightenment would be displayed if {@link
     * #shouldTriggerHelpUI(String)} had been invoked instead.
     */
    boolean wouldTriggerHelpUI(String feature);

    /**
     * This function can be called to query if a particular |feature| has ever been
     * displayed at least once in the past. The days counted is controlled by the
     * EventConfig of "event_trigger".
     * If |from_window| is set to true, the search window size will be set to
     * event_trigger.window; otherwise, the window size will be event_trigger.storage.
     *
     * Calling this method requires the Tracker to already have been initialized.
     * See IsInitialized() and AddOnInitializedCallback(...) for how to ensure
     * the call to this is delayed.
     *
     * @return Whether feature enlightenment has been displayed at least once.
     */
    boolean hasEverTriggered(String feature, boolean fromWindow);

    /**
     * This function can be called to query if a particular |feature| meets its particular
     * precondition for triggering within the bounds of the current feature configuration.
     * Calling this method requires the {@link Tracker} to already have been initialized.
     * See {@link #isInitialized()) and {@link #addOnInitializedCallback(Callback<Boolean>)} for how
     * to ensure the call to this is delayed.
     * This function can typically be used to ensure that expensive operations for tracking other
     * state related to in-product help do not happen if in-product help has already been displayed
     * for the given |feature|.
     */
    @TriggerState
    int getTriggerState(String feature);

    /**
     * Must be called after display of feature enlightenment finishes for a particular feature.
     * @param  feature  the name of the feature dismissing in-product help.
     */
    void dismissed(String feature);

    /**
     * For callers interested in showing a snooze button. For other callers, use the Dismissed(..)
     * method.
     * @param feature The name of the feature dismissing in-product help.
     * @param snoozeAction The action taken by the user on the snooze UI.
     */
    void dismissedWithSnooze(String feature, int snoozeAction);

    /**
     * Acquiring a display lock means that no in-product help can be displayed while it is held. To
     * release the lock, delete the handle. If in-product help is already displayed while the
     * display lock is acquired, the lock is still handed out, but it will not dismiss the current
     * in-product help. However, no new in-product help will be shown until all locks have been
     * released. It is required to invoke {@link DisplayLockHandle#release()} once the lock should
     * no longer be held.
     * The DisplayLockHandle must be released on the main thread.
     * @return a DisplayLockHandle, or {@code null} if no handle could be retrieved.
     */
    @CheckResult
    @Nullable
    DisplayLockHandle acquireDisplayLock();

    /**
     * Called by the client to notify the tracker that a priority notification should be shown. If a
     * handler has already been registered, the IPH will be shown right away. Otherwise, the tracker
     * will cache the priority feature and will show the IPH whenever a handler is registered in
     * future. All other IPHs will be blocked until then.
     */
    void setPriorityNotification(String feature);

    /**
     * Called to check if there is a priority notification scheduled to be shown next. Returns null
     * if there is none scheduled to be shown or the notification has already been shown.
     */
    @Nullable
    String getPendingPriorityNotification();

    /**
     * Called by the client to register a handler for priority notifications. This
     * will essentially contain the code to spin up an IPH. The handler runs only once and
     * unregisters itself.
     */
    void registerPriorityNotificationHandler(String feature, Runnable priorityNotificationHandler);

    /** Unregister the handler. Must be called during client destruction. */
    void unregisterPriorityNotificationHandler(String feature);

    /**
     * Returns whether the tracker has been successfully initialized. During startup, this will be
     * false until the internal models have been loaded at which point it is set to true if the
     * initialization was successful. The state will never change from initialized to uninitialized.
     * Callers can invoke AddOnInitializedCallback(...) to be notified when the result of the
     * initialization is ready.
     */
    boolean isInitialized();

    /**
     * For features that trigger on startup, they can register a callback to ensure that they are
     * informed when the tracker has finished the initialization. If the tracker has already been
     * initialized, the callback will still be invoked with the result. The callback is guaranteed
     * to be invoked exactly one time.
     *
     * The |result| parameter indicates whether the initialization was a success and the tracker is
     * ready to receive calls.
     */
    void addOnInitializedCallback(Callback<Boolean> callback);
}
