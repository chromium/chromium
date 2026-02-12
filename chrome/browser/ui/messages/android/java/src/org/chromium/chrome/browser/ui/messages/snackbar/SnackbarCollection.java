// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.messages.snackbar;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.text.TextUtils;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.DismissalReason;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;

import java.util.ArrayDeque;
import java.util.Deque;
import java.util.Iterator;

/** A data structure that holds all the {@link Snackbar}s managed by {@link SnackbarManager}. */
@NullMarked
class SnackbarCollection {
    private final Deque<Snackbar> mSnackbars = new ArrayDeque<>();
    private final Deque<Snackbar> mPersistentSnackbars = new ArrayDeque<>();

    /**
     * Adds a new snackbar to the collection. If the new snackbar is of
     * {@link Snackbar#TYPE_ACTION} and current snackbar is of
     * {@link Snackbar#TYPE_NOTIFICATION}, the current snackbar will be removed from the
     * collection immediately. Snackbars of {@link Snackbar#TYPE_PERSISTENT} will appear after all
     * action and notification type snackbars are dismissed and will be hidden if an action or
     * notifications are added after it.
     */
    void add(Snackbar snackbar) {
        if (snackbar.isTypeAction()) {
            if (getCurrent() != null && !getCurrent().isTypeAction()) {
                removeCurrent(DismissalReason.REPLACED_BY_ACTION_SNACKBAR);
            }
            mSnackbars.addFirst(snackbar);
        } else if (snackbar.isTypePersistent()) {
            // Although persistent snackbars set their action text by default, it is possible that
            // the developer overrides it. This is a safeguard to ensure all persistent snackbars
            // have a method of dismissal.
            assert !TextUtils.isEmpty(snackbar.getActionText())
                    : "Persistent snackbars require action text.";
            mPersistentSnackbars.addFirst(snackbar);
        } else {
            mSnackbars.addLast(snackbar);
        }
    }

    /**
     * Removes the current snackbar from the collection after the user has clicked on the action
     * button.
     */
    Snackbar removeCurrentDueToAction() {
        return removeCurrent(DismissalReason.ACTION_BUTTON);
    }

    /** Removes the current snackbar from the collection after the user has swiped it away. */
    @Nullable Snackbar removeCurrentDueToSwipe() {
        return removeCurrent(DismissalReason.SWIPE);
    }

    private Snackbar removeCurrent(@DismissalReason int reason) {
        Snackbar current = mSnackbars.pollFirst();
        if (current == null) {
            current = mPersistentSnackbars.pollFirst();
        }
        if (current != null) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Snackbar.DismissalReason", reason, DismissalReason.NUM_ENTRIES);
            SnackbarController controller = current.getController();
            if (controller != null) {
                if (reason == DismissalReason.ACTION_BUTTON) {
                    controller.onAction(current.getActionData());
                } else {
                    controller.onDismissNoAction(current.getActionData());
                }
            }
        }
        return current;
    }

    /**
     * @return The snackbar that is currently displayed.
     */
    Snackbar getCurrent() {
        Snackbar current = mSnackbars.peekFirst();
        if (current == null) {
            current = mPersistentSnackbars.peekFirst();
        }
        return current;
    }

    boolean isEmpty() {
        return mSnackbars.isEmpty() && mPersistentSnackbars.isEmpty();
    }

    void clear() {
        while (!isEmpty()) {
            removeCurrent(DismissalReason.OTHERS);
        }
    }

    void removeCurrentDueToTimeout() {
        Snackbar current = getCurrent();
        if (current.isTypePersistent()) {
            // In theory, this method should never be called on a persistent snackbar as the
            // dismissal handler is disabled. As a precaution, we exit early if the snackbar is
            // meant to be persistent.
            return;
        }
        removeCurrent(DismissalReason.TIMEOUT);
        while ((current = getCurrent()) != null && current.isTypeAction()) {
            removeCurrent(DismissalReason.TIMEOUT);
        }
    }

    boolean removeMatchingSnackbars(SnackbarController controller) {
        return removeSnackbarFromList(mSnackbars, controller, DismissalReason.DISMISSED_BY_CALLER)
                || removeSnackbarFromList(
                        mPersistentSnackbars, controller, DismissalReason.DISMISSED_BY_CALLER);
    }

    private static boolean removeSnackbarFromList(
            Deque<Snackbar> list, SnackbarController controller, @DismissalReason int reason) {
        if (controller == null) return false;
        boolean snackbarRemoved = false;
        Iterator<Snackbar> iter = list.iterator();
        while (iter.hasNext()) {
            Snackbar snackbar = iter.next();

            if (snackbar.getController() != controller) continue;

            iter.remove();
            RecordHistogram.recordEnumeratedHistogram(
                    "Snackbar.DismissalReason", reason, DismissalReason.NUM_ENTRIES);
            controller.onDismissNoAction(snackbar.getActionData());
            snackbarRemoved = true;
        }
        return snackbarRemoved;
    }

    boolean removeMatchingSnackbars(SnackbarController controller, Object data) {
        return removeSnackbarFromList(
                        mSnackbars, controller, data, DismissalReason.DISMISSED_BY_CALLER)
                || removeSnackbarFromList(
                        mPersistentSnackbars,
                        controller,
                        data,
                        DismissalReason.DISMISSED_BY_CALLER);
    }

    private static boolean removeSnackbarFromList(
            Deque<Snackbar> list,
            SnackbarController controller,
            Object data,
            @DismissalReason int reason) {
        if (controller == null) return false;
        boolean snackbarRemoved = false;
        Iterator<Snackbar> iter = list.iterator();
        while (iter.hasNext()) {
            Snackbar snackbar = iter.next();

            if (snackbar.getController() != controller) continue;
            if (!objectsAreEqual(snackbar.getActionData(), data)) continue;

            iter.remove();
            RecordHistogram.recordEnumeratedHistogram(
                    "Snackbar.DismissalReason", reason, DismissalReason.NUM_ENTRIES);
            controller.onDismissNoAction(assumeNonNull(snackbar.getActionData()));
            snackbarRemoved = true;
        }
        return snackbarRemoved;
    }

    private static boolean objectsAreEqual(@Nullable Object a, @Nullable Object b) {
        if (a == null && b == null) return true;
        if (a == null || b == null) return false;
        return a.equals(b);
    }
}
