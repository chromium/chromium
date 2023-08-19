// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.messages.snackbar;

import android.app.Activity;
import android.os.Handler;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.UnownedUserData;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.WindowAndroid;

/**
 * Manager for the snackbar showing at the bottom of activity. There should be only one
 * SnackbarManager and one snackbar in the activity.
 * <p/>
 * When action button is clicked, this manager will call {@link SnackbarController#onAction(Object)}
 * in corresponding listener, and show the next entry. Otherwise if no action is taken by user
 * during {@link #DEFAULT_SNACKBAR_DURATION_MS} milliseconds, it will call
 * {@link SnackbarController#onDismissNoAction(Object)}. Note, snackbars of
 * {@link Snackbar#TYPE_PERSISTENT} do not get automatically dismissed after a timeout.
 */
public class SnackbarManager implements OnClickListener, ActivityStateListener, UnownedUserData {
    /**
     * Interface that shows the ability to provide a snackbar manager.
     */
    public interface SnackbarManageable {
        /**
         * @return The snackbar manager that has a proper anchor view.
         */
        SnackbarManager getSnackbarManager();
    }

    /**
     * Controller that post entries to snackbar manager and interact with snackbar manager during
     * dismissal and action click event.
     */
    public interface SnackbarController {
        /**
         * Called when the user clicks the action button on the snackbar.
         * @param actionData Data object passed when showing this specific snackbar.
         */
        default void onAction(Object actionData) {}

        /**
         * Called when the snackbar is dismissed by timeout or UI environment change.
         * @param actionData Data object associated with the dismissed snackbar entry.
         */
        default void onDismissNoAction(Object actionData) {}
    }

    public static final int DEFAULT_SNACKBAR_DURATION_MS = 3000;
    // For snackbars with long strings where a longer duration is favorable.
    public static final int DEFAULT_SNACKBAR_DURATION_LONG_MS = 8000;
    private static final int ACCESSIBILITY_MODE_SNACKBAR_DURATION_MS = 30000;

    // Used instead of the constant so tests can override the value.
    private static int sSnackbarDurationMs = DEFAULT_SNACKBAR_DURATION_MS;
    private static int sAccessibilitySnackbarDurationMs = ACCESSIBILITY_MODE_SNACKBAR_DURATION_MS;

    private Activity mActivity;
    private SnackbarView mView;
    private final Handler mUIThreadHandler;
    private SnackbarCollection mSnackbars = new SnackbarCollection();
    private boolean mActivityInForeground;
    private boolean mIsDisabledForTesting;
    private ViewGroup mSnackbarParentView;
    private ViewGroup mSnackbarTemporaryParentView;
    private final WindowAndroid mWindowAndroid;
    private final Runnable mHideRunnable = new Runnable() {
        @Override
        public void run() {
            mSnackbars.removeCurrentDueToTimeout();
            updateView();
        }
    };
    private final ObservableSupplierImpl<Boolean> mIsShowingSupplier =
            new ObservableSupplierImpl<>();

    /**
     * Constructs a SnackbarManager to show snackbars in the given window.
     * @param activity The embedding activity.
     * @param snackbarParentView The ViewGroup used to display this snackbar.
     * @param windowAndroid The WindowAndroid used for starting animation. If it is null,
     *                      Animator#start is called instead.
     */
    public SnackbarManager(Activity activity, ViewGroup snackbarParentView,
            @Nullable WindowAndroid windowAndroid) {
        mActivity = activity;
        mUIThreadHandler = new Handler();
        mSnackbarParentView = snackbarParentView;
        mWindowAndroid = windowAndroid;

        ApplicationStatus.registerStateListenerForActivity(this, mActivity);
        if (ApplicationStatus.getStateForActivity(mActivity) == ActivityState.STARTED
                || ApplicationStatus.getStateForActivity(mActivity) == ActivityState.RESUMED) {
            onStart();
        }

        mIsShowingSupplier.set(isShowing());
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        assert activity == mActivity;
        if (newState == ActivityState.STARTED) {
            onStart();
        } else if (newState == ActivityState.STOPPED) {
            onStop();
        }
    }

    /**
     * Notifies the snackbar manager that the activity is running in foreground now.
     */
    private void onStart() {
        mActivityInForeground = true;
    }

    /**
     * Notifies the snackbar manager that the activity has been pushed to background.
     */
    private void onStop() {
        mSnackbars.clear();
        updateView();
        mActivityInForeground = false;
    }

    /**
     * @return True if a Snackbar can currently be shown by this SnackbarManager.
     */
    public boolean canShowSnackbar() {
        return mActivityInForeground && !mIsDisabledForTesting;
    }

    /**
     * Shows a snackbar at the bottom of the screen, or above the keyboard if the keyboard is
     * visible.
     */
    public void showSnackbar(Snackbar snackbar) {
        if (!mActivityInForeground || mIsDisabledForTesting) return;
        RecordHistogram.recordSparseHistogram("Snackbar.Shown", snackbar.getIdentifier());

        mSnackbars.add(snackbar);
        updateView();
        mView.announceforAccessibility();
    }

    /** Dismisses all snackbars. */
    public void dismissAllSnackbars() {
        if (mSnackbars.isEmpty()) return;

        mSnackbars.clear();
        updateView();
    }

    /**
     * Dismisses snackbars that are associated with the given {@link SnackbarController}.
     *
     * @param controller Only snackbars with this controller will be removed. A snackbar associated
     *         with a null controller cannot be dismissed via this method.
     */
    public void dismissSnackbars(SnackbarController controller) {
        if (mSnackbars.removeMatchingSnackbars(controller)) {
            updateView();
        }
    }

    /**
     * Dismisses snackbars that have a certain controller and action data.
     *
     * @param controller Only snackbars with this controller will be removed. A snackbar associated
     *         with a null controller cannot be dismissed via this method.
     * @param actionData Only snackbars whose action data is equal to actionData will be removed.
     */
    public void dismissSnackbars(SnackbarController controller, Object actionData) {
        if (mSnackbars.removeMatchingSnackbars(controller, actionData)) {
            updateView();
        }
    }

    /**
     * Handles click event for action button at end of snackbar.
     */
    @Override
    public void onClick(View v) {
        mView.announceActionForAccessibility();
        mSnackbars.removeCurrentDueToAction();
        updateView();
    }

    /**
     * After an infobar is added, brings snackbar view above it.
     * TODO(crbug/1028382): Currently SnackbarManager doesn't observe InfobarContainer events.
     * Restore this functionality, only without references to Infobar classes.
     */
    public void onAddInfoBar() {
        // Bring Snackbars to the foreground so that it's not blocked by infobars.
        if (isShowing()) {
            mView.bringToFront();
        }
    }

    /**
     * Overrides the parent {@link ViewGroup} of the currently-showing snackbar. This method removes
     * the snackbar from its original parent, and attaches it to the given parent. If
     * <code>null</code> is given, the snackbar will be reattached to its original parent.
     *
     * @param overridingParent The overriding parent for the current snackbar. If null, previous
     *                         calls of this method will be reverted.
     */
    public void overrideParent(ViewGroup overridingParent) {
        if (mView != null) mView.overrideParent(overridingParent);
    }

    /**
     * Changes the parent {@link ViewGroup} for snackbars (including the currently showing snackbar,
     * if it exists). If <code>null</code> is given, snackbars will once again be attached to the
     * original parent.
     *
     * @param parentView The new parent for snackbars. If null, previous calls of this
     *                   method will be reverted.
     */
    public void setParentView(ViewGroup parentView) {
        mSnackbarTemporaryParentView = parentView;
        overrideParent(mSnackbarTemporaryParentView);
    }

    /**
     * @return Supplier of whether the snackbar is showing
     */
    public ObservableSupplier<Boolean> isShowingSupplier() {
        return mIsShowingSupplier;
    }

    /**
     * @return Whether there is a snackbar on screen.
     */
    public boolean isShowing() {
        return mView != null && mView.isShowing();
    }

    /**
     * Updates the {@link SnackbarView} to reflect the value of mSnackbars.currentSnackbar(), which
     * may be null. This might show, change, or hide the view.
     */
    private void updateView() {
        if (!mActivityInForeground) return;
        Snackbar currentSnackbar = mSnackbars.getCurrent();
        if (currentSnackbar == null) {
            mUIThreadHandler.removeCallbacks(mHideRunnable);
            if (mView != null) {
                mView.dismiss();
                mView = null;
            }
        } else {
            boolean viewChanged = true;
            if (mView == null) {
                mView = new SnackbarView(
                        mActivity, this, currentSnackbar, mSnackbarParentView, mWindowAndroid);
                mView.show();

                // If there is a temporary parent set, reparent accordingly. We override here
                // instead of instantiating the new SnackbarView with the temporary parent, so
                // that overriding with <code>null</code> will reparent to mSnackbarParentView.
                if (mSnackbarTemporaryParentView != null) {
                    mView.overrideParent(mSnackbarTemporaryParentView);
                }
            } else {
                viewChanged = mView.update(currentSnackbar);
            }

            if (viewChanged) {
                mUIThreadHandler.removeCallbacks(mHideRunnable);
                if (!currentSnackbar.isTypePersistent()) {
                    int durationMs = getDuration(currentSnackbar);
                    mUIThreadHandler.postDelayed(mHideRunnable, durationMs);
                }
                mView.announceforAccessibility();
            }
        }

        mIsShowingSupplier.set(isShowing());
    }

    @VisibleForTesting
    int getDuration(Snackbar snackbar) {
        int durationMs = snackbar.getDuration();
        if (durationMs == 0) durationMs = sSnackbarDurationMs;

        // If no a11y service that can perform gestures is enabled, use the set duration. Otherwise
        // multiply the duration by the recommended multiplier and use that with a minimum of 30s.
        return !AccessibilityState.isPerformGesturesEnabled()
                ? durationMs
                : AccessibilityState.getRecommendedTimeoutMillis(
                        sAccessibilitySnackbarDurationMs, durationMs);
    }

    /**
     * Disables the snackbar manager. This is only intended for testing purposes.
     */
    public void disableForTesting() {
        mIsDisabledForTesting = true;
    }

    /**
     * Overrides the default snackbar duration with a custom value for testing.
     * @param durationMs The duration to use in ms.
     */
    public static void setDurationForTesting(int durationMs) {
        sSnackbarDurationMs = durationMs;
        sAccessibilitySnackbarDurationMs = durationMs;
    }

    /**
     * Clears any overrides set for testing.
     */
    public static void resetDurationForTesting() {
        sSnackbarDurationMs = DEFAULT_SNACKBAR_DURATION_MS;
        sAccessibilitySnackbarDurationMs = ACCESSIBILITY_MODE_SNACKBAR_DURATION_MS;
    }

    static int getDefaultDurationForTesting() {
        return sSnackbarDurationMs;
    }

    static int getDefaultA11yDurationForTesting() {
        return sAccessibilitySnackbarDurationMs;
    }

    /**
     * @return The currently showing snackbar. For testing only.
     */
    public Snackbar getCurrentSnackbarForTesting() {
        return mSnackbars.getCurrent();
    }

    /**
     * @return The currently showing snackbar view. For testing only.
     */
    public SnackbarView getCurrentSnackbarViewForTesting() {
        return mView;
    }
}
