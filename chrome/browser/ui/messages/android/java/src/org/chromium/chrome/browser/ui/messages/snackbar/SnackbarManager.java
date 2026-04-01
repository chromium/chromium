// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.messages.snackbar;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.os.Handler;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Manager for the snackbar showing at the bottom of activity. There should be only one
 * SnackbarManager and one snackbar in the activity.
 *
 * <p>When action button is clicked, this manager will call {@link
 * SnackbarController#onAction(Object)} in corresponding listener, and show the next entry.
 * Otherwise if no action is taken by user during {@link #DEFAULT_SNACKBAR_DURATION_MS}
 * milliseconds, it will call {@link SnackbarController#onDismissNoAction(Object)}. Note, snackbars
 * of {@link Snackbar#TYPE_PERSISTENT} do not get automatically dismissed after a timeout.
 *
 * <p>If a modal dialog is shown, any snackbars that are already showing or that begin to show will
 * wait until all modal dialogs are dismissed and then will begin their timeout countdowns.
 */
@NullMarked
public class SnackbarManager
        implements OnClickListener,
                ActivityStateListener,
                InsetObserver.WindowInsetObserver,
                ModalDialogManagerObserver {
    @IntDef({
        DismissalReason.UNKNOWN,
        DismissalReason.ACTION_BUTTON,
        DismissalReason.TIMEOUT,
        DismissalReason.SWIPE,
        DismissalReason.DISMISSED_BY_CALLER,
        DismissalReason.REPLACED_BY_ACTION_SNACKBAR,
        DismissalReason.OTHERS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DismissalReason {
        int UNKNOWN = 0;
        int ACTION_BUTTON = 1;
        int TIMEOUT = 2;
        int SWIPE = 3;
        int DISMISSED_BY_CALLER = 4;
        int REPLACED_BY_ACTION_SNACKBAR = 5;
        int OTHERS = 6;
        int NUM_ENTRIES = 7;
    }

    // The slot to push parent view overrides to. An entry with a larger number will take
    // precedence. For example, if HUB and ONE_OFF are both present, ONE_OFF will be used. However,
    // if ONE_OFF is then removed, HUB will be used.
    //
    // Note: ONE_OFF can be used for one off overrides that are known to be atop other overrides or
    // are a set once and never removed case.
    @IntDef({
        ParentOverrideSlot.HUB,
        ParentOverrideSlot.TAB_LIST_EDITOR,
        ParentOverrideSlot.ARCHIVED_TABS_DIALOG,
        ParentOverrideSlot.ONE_OFF,
        ParentOverrideSlot.NUM_ENTRIES
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    public @interface ParentOverrideSlot {
        int HUB = 0;
        int TAB_LIST_EDITOR = 1;
        int ARCHIVED_TABS_DIALOG = 2;
        int ONE_OFF = 3; // LAST
        int NUM_ENTRIES = 4;
    }

    /** Interface that shows the ability to provide a snackbar manager. */
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
         *
         * @param actionData Data object passed when showing this specific snackbar. Will be null if
         *     action data was not set on the snackbar.
         */
        default void onAction(@Nullable Object actionData) {}

        /**
         * Called when the snackbar is dismissed by timeout or UI environment change.
         *
         * @param actionData Data object associated with the dismissed snackbar entry. Will be null
         *     if action data was not set on the snackbar.
         */
        default void onDismissNoAction(@Nullable Object actionData) {}
    }

    private static class OverridingContext {
        public final ViewGroup parentView;
        public final NonNullObservableSupplier<Integer> additionalBottomMarginPxSupplier;

        OverridingContext(
                ViewGroup parentView,
                NonNullObservableSupplier<Integer> additionalBottomMarginPxSupplier) {
            this.parentView = parentView;
            this.additionalBottomMarginPxSupplier = additionalBottomMarginPxSupplier;
        }
    }

    public static final int DEFAULT_SNACKBAR_DURATION_MS = 3000;
    // For snackbars with long strings where a longer duration is favorable.
    public static final int DEFAULT_SNACKBAR_DURATION_LONG_MS = 8000;
    public static final int DEFAULT_TYPE_ACTION_SNACKBAR_DURATION_MS = 10000;
    private static final int ACCESSIBILITY_MODE_SNACKBAR_DURATION_MS = 30000;

    // Used instead of the constant so tests can override the value.
    private static int sSnackbarDurationMs = DEFAULT_SNACKBAR_DURATION_MS;
    private static int sAccessibilitySnackbarDurationMs = ACCESSIBILITY_MODE_SNACKBAR_DURATION_MS;
    private static int sTypeActionSnackbarDurationsMs = DEFAULT_TYPE_ACTION_SNACKBAR_DURATION_MS;

    private final Activity mActivity;
    private final @Nullable WindowAndroid mWindowAndroid;
    private final Handler mUiThreadHandler;
    private final Runnable mHideRunnable =
            new Runnable() {
                @Override
                public void run() {
                    mSnackbars.removeCurrentDueToTimeout();
                    updateView();
                }
            };
    private final SettableNonNullObservableSupplier<Boolean> mIsShowingSupplier;
    private final ViewGroup mOriginalParentView;
    private final @Nullable OverridingContext[] mParentOverrideSlots =
            new OverridingContext[ParentOverrideSlot.NUM_ENTRIES];
    private final SnackbarCollection mSnackbars = new SnackbarCollection();
    private final NonNullObservableSupplier<Integer> mAdditionalBottomMarginPxSupplier;
    private final @Nullable ModalDialogManager mModalDialogManager;

    private @Nullable SnackbarView mView;
    private boolean mActivityInForeground;
    private boolean mIsDisabledForTesting;
    private boolean mIsModalDialogShowing;

    /**
     * Constructs a SnackbarManager to show snackbars in the given window.
     *
     * @param activity The embedding activity.
     * @param snackbarParentView The ViewGroup used to display this snackbar.
     * @param windowAndroid The WindowAndroid used for starting animation. If it is null,
     *     Animator#start is called instead.
     * @param additionalBottomMarginPxSupplier The supplier publishes the changes of the additional
     *     bottom margin in pixels.
     * @param modalDialogManager The ModalDialogManager to observe for dialog visibility.
     */
    public SnackbarManager(
            Activity activity,
            ViewGroup snackbarParentView,
            @Nullable WindowAndroid windowAndroid,
            @Nullable NonNullObservableSupplier<Integer> additionalBottomMarginPxSupplier,
            @Nullable ModalDialogManager modalDialogManager) {
        mActivity = activity;
        mUiThreadHandler = new Handler();
        mOriginalParentView = snackbarParentView;
        mWindowAndroid = windowAndroid;
        if (additionalBottomMarginPxSupplier != null) {
            mAdditionalBottomMarginPxSupplier = additionalBottomMarginPxSupplier;
        } else {
            mAdditionalBottomMarginPxSupplier = ObservableSuppliers.createNonNull(0);
        }

        ApplicationStatus.registerStateListenerForActivity(this, mActivity);
        if (ApplicationStatus.getStateForActivity(mActivity) == ActivityState.STARTED
                || ApplicationStatus.getStateForActivity(mActivity) == ActivityState.RESUMED) {
            onStart();
        }
        mIsShowingSupplier = ObservableSuppliers.createNonNull(isShowing());

        mModalDialogManager = modalDialogManager;
        if (mModalDialogManager != null) {
            mModalDialogManager.addObserver(this);
            mIsModalDialogShowing = mModalDialogManager.isShowing();
        }
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

    /** Notifies the snackbar manager that the activity is running in foreground now. */
    private void onStart() {
        mActivityInForeground = true;
    }

    /** Notifies the snackbar manager that the activity has been pushed to background. */
    private void onStop() {
        mSnackbars.clear();
        updateView();
        mActivityInForeground = false;
    }

    /** Destroys the SnackbarManager, unregistering any observers and dismissing all snackbars. */
    public void destroy() {
        dismissAllSnackbars();
        ApplicationStatus.unregisterActivityStateListener(this);
        if (mModalDialogManager != null) {
            mModalDialogManager.removeObserver(this);
        }
        mUiThreadHandler.removeCallbacks(mHideRunnable);
    }

    /**
     * @return True if a Snackbar can currently be shown by this SnackbarManager.
     */
    public boolean canShowSnackbar() {
        return mActivityInForeground && !mIsDisabledForTesting;
    }

    /** Shows a snackbar at the bottom of the screen. */
    public void showSnackbar(Snackbar snackbar) {
        if (!mActivityInForeground || mIsDisabledForTesting) return;
        RecordHistogram.recordSparseHistogram("Snackbar.Shown", snackbar.getIdentifier());

        mSnackbars.add(snackbar);
        updateView();
        assumeNonNull(mView);
        mView.updateAccessibilityPaneTitle();
    }

    /** Dismisses the currently showing snackbar after user swipe. */
    void dismissCurrentSnackbarDueToSwipe() {
        if (mSnackbars.isEmpty()) return;
        mSnackbars.removeCurrentDueToSwipe();
        updateView();
    }

    /** Dismisses the currently showing snackbar. */
    void dismissCurrentSnackbar() {
        if (mSnackbars.isEmpty()) return;
        SnackbarController currentSnackbarController = mSnackbars.getCurrent().getController();
        assertNonNull(currentSnackbarController);
        dismissSnackbars(currentSnackbarController);
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

    /** Resets the timeout to hide the snackbar. */
    public void resetSnackbarTimeout() {
        mUiThreadHandler.removeCallbacks(mHideRunnable);
        Snackbar currentSnackbar = mSnackbars.getCurrent();
        if (currentSnackbar != null
                && !currentSnackbar.isTypePersistent()
                && !mIsModalDialogShowing) {
            int durationMs = getDuration(currentSnackbar);
            mUiThreadHandler.postDelayed(mHideRunnable, durationMs);
        }
    }

    /** Handles click event for action button at end of snackbar. */
    @Override
    public void onClick(View v) {
        mSnackbars.removeCurrentDueToAction();
        updateView();
    }

    @Override
    public void onDialogShown(View dialogView) {
        mIsModalDialogShowing = true;
        mUiThreadHandler.removeCallbacks(mHideRunnable);
    }

    @Override
    public void onLastDialogDismissed() {
        mIsModalDialogShowing = false;
        resetSnackbarTimeout();
    }

    /**
     * After an infobar is added, brings snackbar view above it. TODO(crbug.com/40109125): Currently
     * SnackbarManager doesn't observe InfobarContainer events. Restore this functionality, only
     * without references to Infobar classes.
     */
    public void onAddInfoBar() {
        // Bring Snackbars to the foreground so that it's not blocked by infobars.
        if (isShowing()) {
            assumeNonNull(mView).bringToFront();
        }
    }

    /**
     * Pushes the given {@link ViewGroup} onto the override slots. The highest priority slot will be
     * used for the current snackbar.
     *
     * @param slot The slot to push the override to.
     * @param parentView The new parent for snackbars, must be non-null.
     * @param additionalBottomMarginPxSupplier The supplier publishes the changes of the additional
     *     bottom margin in pixels. Passing null will use the default behavior of the root parent.
     */
    public void pushParentViewOverride(
            @ParentOverrideSlot int slot,
            ViewGroup parentView,
            @Nullable NonNullObservableSupplier<Integer> additionalBottomMarginPxSupplier) {
        assert parentView != null;
        assert slot < mParentOverrideSlots.length;
        // Allow ONE_OFF to be reused for several cases where we just override and forget in
        // SnackbarActivity, etc.
        assert slot == ParentOverrideSlot.ONE_OFF || mParentOverrideSlots[slot] == null
                : "Slot " + slot + " is already in use.";

        var nonNullAdditionalBottomMarginPxSupplier =
                additionalBottomMarginPxSupplier == null
                        ? mAdditionalBottomMarginPxSupplier
                        : additionalBottomMarginPxSupplier;
        mParentOverrideSlots[slot] =
                new OverridingContext(parentView, nonNullAdditionalBottomMarginPxSupplier);
        updateParentViewOverride();
    }

    /**
     * Pops the {@link ViewGroup} corresponding to the given slot from the override slots. Updates
     * any visible snackbars to a new parent if necessary.
     *
     * @param slot The slot to pop the override from.
     */
    public void popParentViewOverride(@ParentOverrideSlot int slot) {
        assert mParentOverrideSlots[slot] != null : "Slot " + slot + " was not in use.";
        mParentOverrideSlots[slot] = null;
        updateParentViewOverride();
    }

    private void updateParentViewOverride() {
        for (int i = ParentOverrideSlot.NUM_ENTRIES - 1; i >= 0; i--) {
            var overridingContext = mParentOverrideSlots[i];
            if (overridingContext != null) {
                overrideParent(
                        overridingContext.parentView,
                        overridingContext.additionalBottomMarginPxSupplier);
                return;
            }
        }
        overrideParent(mOriginalParentView, mAdditionalBottomMarginPxSupplier);
    }

    /**
     * @return Supplier of whether the snackbar is showing
     */
    public NonNullObservableSupplier<Boolean> isShowingSupplier() {
        return mIsShowingSupplier;
    }

    /**
     * @return Whether there is a snackbar on screen.
     */
    public boolean isShowing() {
        return mView != null && mView.isShowing();
    }

    /**
     * Overrides the parent {@link ViewGroup} of the currently-showing snackbar. This method removes
     * the snackbar from its original parent, and attaches it to the given parent.
     *
     * @param overridingParent The overriding parent for the current snackbar.
     * @param additionalBottomMarginPxSupplier The supplier publishes the changes of the additional
     *     bottom margin in pixels. May be null to use the default behavior of the parent.
     */
    @VisibleForTesting
    void overrideParent(
            ViewGroup overridingParent,
            NonNullObservableSupplier<Integer> additionalBottomMarginPxSupplier) {
        if (mView != null) mView.overrideParent(overridingParent, additionalBottomMarginPxSupplier);
    }

    /**
     * Updates the {@link SnackbarView} to reflect the value of mSnackbars.currentSnackbar(), which
     * may be null. This might show, change, or hide the view.
     */
    private void updateView() {
        if (!mActivityInForeground) return;
        Snackbar currentSnackbar = mSnackbars.getCurrent();
        if (currentSnackbar == null) {
            mUiThreadHandler.removeCallbacks(mHideRunnable);
            if (mView != null) {
                mView.dismiss();
                mView = null;
            }
        } else {
            boolean viewChanged = true;
            if (mView == null) {
                mView =
                        new SnackbarView(
                                mActivity,
                                this,
                                currentSnackbar,
                                mOriginalParentView,
                                mWindowAndroid,
                                mAdditionalBottomMarginPxSupplier);
                mView.show();

                // If there is a temporary parent set, reparent accordingly. We override here
                // instead of instantiating the new SnackbarView with the temporary parent, so
                // that overriding with <code>null</code> will reparent to mSnackbarParentView.
                updateParentViewOverride();
            } else {
                viewChanged = mView.update(currentSnackbar);
            }

            if (viewChanged) {
                mUiThreadHandler.removeCallbacks(mHideRunnable);
                if (!currentSnackbar.isTypePersistent() && !mIsModalDialogShowing) {
                    int durationMs = getDuration(currentSnackbar);
                    mUiThreadHandler.postDelayed(mHideRunnable, durationMs);
                }
                mView.updateAccessibilityPaneTitle();
            }
        }

        mIsShowingSupplier.set(isShowing());
    }

    // ============================================================================================
    // Testing
    // ============================================================================================

    @VisibleForTesting
    int getDuration(Snackbar snackbar) {
        int durationMs = Math.max(snackbar.getDuration(), sSnackbarDurationMs);
        if (snackbar.isTypeAction()) {
            durationMs = Math.max(sTypeActionSnackbarDurationsMs, durationMs);
        }

        // If a11y is on, set a longer minimum duration; otherwise, use the recommended timeout
        // duration.
        int minDuration =
                AccessibilityState.isPerformGesturesEnabled()
                        ? sAccessibilitySnackbarDurationMs
                        : durationMs;

        return AccessibilityState.getRecommendedTimeoutMillis(minDuration, durationMs);
    }

    /** Disables the snackbar manager. This is only intended for testing purposes. */
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
        sTypeActionSnackbarDurationsMs = durationMs;
        ResettersForTesting.register(SnackbarManager::resetDurationForTesting);
    }

    /** Clears any overrides set for testing. */
    public static void resetDurationForTesting() {
        sSnackbarDurationMs = DEFAULT_SNACKBAR_DURATION_MS;
        sAccessibilitySnackbarDurationMs = ACCESSIBILITY_MODE_SNACKBAR_DURATION_MS;
        sTypeActionSnackbarDurationsMs = DEFAULT_TYPE_ACTION_SNACKBAR_DURATION_MS;
    }

    static int getDefaultDurationForTesting() {
        return sSnackbarDurationMs;
    }

    static int getDefaultA11yDurationForTesting() {
        return sAccessibilitySnackbarDurationMs;
    }

    static int getDefaultTypeActionSnackbarDuration() {
        return sTypeActionSnackbarDurationsMs;
    }

    /**
     * @return The currently showing snackbar. For testing only.
     */
    public Snackbar getCurrentSnackbarForTesting() {
        return mSnackbars.getCurrent();
    }

    /** Returns the currently showing snackbar view. For testing only. */
    public @Nullable SnackbarView getCurrentSnackbarViewForTesting() {
        return mView;
    }
}
