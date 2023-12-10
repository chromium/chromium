// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.messages.snackbar;

import android.text.TextUtils;

import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;

import java.util.Deque;
import java.util.Iterator;
import java.util.LinkedList;

/** A data structure that holds all the {@link Snackbar}s managed by {@link SnackbarManager}. */
class SnackbarCollection {
    private Deque<Snackbar> mSnackbars = new LinkedList<>();
    private Deque<Snackbar> mPersistentSnackbars = new LinkedList<>();

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
                removeCurrent(false);
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
        return removeCurrent(true);
    }

    private Snackbar removeCurrent(boolean isAction) {
        Snackbar current = mSnackbars.pollFirst();
        if (current == null) {
            current = mPersistentSnackbars.pollFirst();
        }
        if (current != null) {
            SnackbarController controller = current.getController();
            if (controller != null) {
                if (isAction) {
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
            removeCurrent(false);
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
        removeCurrent(false);
        while ((current = getCurrent()) != null && current.isTypeAction()) {
            removeCurrent(false);
        }
    }

    boolean removeMatchingSnackbars(SnackbarController controller) {
        return removeSnackbarFromList(mSnackbars, controller)
                || removeSnackbarFromList(mPersistentSnackbars, controller);
    }

    private static boolean removeSnackbarFromList(
            Deque<Snackbar> list, SnackbarController controller) {
        if (controller == null) return false;
        boolean snackbarRemoved = false;
        Iterator<Snackbar> iter = list.iterator();
        while (iter.hasNext()) {
            Snackbar snackbar = iter.next();

            if (snackbar.getController() != controller) continue;

            iter.remove();
            controller.onDismissNoAction(snackbar.getActionData());
            snackbarRemoved = true;
        }
        return snackbarRemoved;
    }

    boolean removeMatchingSnackbars(SnackbarController controller, Object data) {
        return removeSnackbarFromList(mSnackbars, controller, data)
                || removeSnackbarFromList(mPersistentSnackbars, controller, data);
    }

    private static boolean removeSnackbarFromList(
            Deque<Snackbar> list, SnackbarController controller, Object data) {
        if (controller == null) return false;
        boolean snackbarRemoved = false;
        Iterator<Snackbar> iter = list.iterator();
        while (iter.hasNext()) {
            Snackbar snackbar = iter.next();

            if (snackbar.getController() != controller) continue;
            if (!objectsAreEqual(snackbar.getActionData(), data)) continue;

            iter.remove();
            controller.onDismissNoAction(snackbar.getActionData());
            snackbarRemoved = true;
        }
        return snackbarRemoved;
    }

    private static boolean objectsAreEqual(Object a, Object b) {
        if (a == null && b == null) return true;
        if (a == null || b == null) return false;
        return a.equals(b);
    }
}
