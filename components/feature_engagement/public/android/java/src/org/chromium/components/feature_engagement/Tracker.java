// Copyright 2017 The Chromium Authors. All rights reserved.
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
    /**
     * A handle for the display lock. While this is unreleased, no in-product help can be displayed.
     */
    interface DisplayLockHandle {
        /**
         * This method must be invoked when the lock should be released, and it must be invoked on
         * the main thread.
         */
        void release();
    }

    /**
     * Must be called whenever an event happens.
     */
    void notifyEvent(String event);

    /**
     * This function must be called whenever the triggering condition for a specific feature
     * happens. Returns true iff the display of the in-product help must happen.
     * If {@code true} is returned, the caller *must* call {@link #dismissed(String)} when display
     * of feature enlightenment ends.
     *
     * @return whether feature enlightenment should be displayed.
     */
    @CheckResult
    boolean shouldTriggerHelpUI(String feature);

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
     * @return whether feature enlightenment would be displayed if {@link
     * #shouldTriggerHelpUI(String)} had been invoked instead.
     */
    boolean wouldTriggerHelpUI(String feature);

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
     */
    void dismissed(String feature);

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
