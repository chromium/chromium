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
import java.util.ArrayList;
import java.util.Deque;
import java.util.Iterator;
import java.util.List;
import java.util.Objects;

/** A data structure that holds all the {@link Snackbar}s managed by {@link SnackbarManager}. */
@NullMarked
class SnackbarCollection {
    // Standard FIFO queue for normal notifications (e.g., "Copied to clipboard").
    private final Deque<Snackbar> mSnackbars = new ArrayDeque<>();
    // Queue for persistent notifications (e.g., "Offline mode").
    private final Deque<Snackbar> mPersistentSnackbars = new ArrayDeque<>();
    // TODO(crbug.com/564820082): Explore alternatives to handle preemption and resumption across
    // different notice lifecycles (transient countdown vs. persistent state).
    // Holds a transient High Priority security warning (e.g., Fullscreen ExclusiveAccessBubble).
    // Takes precedence over persistent disclosures so its timeout can run immediately.
    private @Nullable Snackbar mActiveTransientHighPriority;
    // Holds a persistent High Priority disclosure (e.g., TWA / WebAPK "Running in Chrome").
    // Displayed whenever no transient HP warning is active, completely isolated from standard
    // queues.
    private @Nullable Snackbar mActivePersistentHighPriority;

    private static final int MAX_SNACKBARS = 10;

    void add(Snackbar snackbar) {
        RecordHistogram.recordExactLinearHistogram(
                "Snackbar.QueueDepthAtInsertion",
                mSnackbars.size()
                        + mPersistentSnackbars.size()
                        + (mActiveTransientHighPriority != null ? 1 : 0)
                        + (mActivePersistentHighPriority != null ? 1 : 0),
                MAX_SNACKBARS + 1);

        if (snackbar.isHighPriority()) {
            Snackbar currentHpInSlot =
                    snackbar.isTypePersistent()
                            ? mActivePersistentHighPriority
                            : mActiveTransientHighPriority;
            if (currentHpInSlot != null) {
                // State Deduplication: Rapid oscillation attacks (e.g., spamming requestFullscreen)
                // are collapsed here. If it's the exact same warning, we ignore the duplicate.
                if (currentHpInSlot.getController() == snackbar.getController()
                        && Objects.equals(
                                currentHpInSlot.getActionData(), snackbar.getActionData())) {
                    // Defensive update: If a controller submits a new Snackbar instance with
                    // modified content (e.g., updated text) instead of mutating in place,
                    // update the active instance rather than dropping the update as a duplicate.
                    if (!currentHpInSlot.equals(snackbar)) {
                        setHighPrioritySlot(snackbar);
                    }
                    return; // Deduplication: already have this HP snackbar.
                }
                // HP snackbars of the same rank replace each other.
                RecordHistogram.recordEnumeratedHistogram(
                        "Snackbar.DismissalReason",
                        DismissalReason.REPLACED_BY_HIGH_PRIORITY,
                        DismissalReason.NUM_ENTRIES);
                assert currentHpInSlot.getController() != null;
                currentHpInSlot.getController().onDismissNoAction(currentHpInSlot.getActionData());
            }
            // Assign to its dedicated HP rank slot without disturbing the other HP rank slot.
            setHighPrioritySlot(snackbar);
            return;
        }

        if (snackbar.isTypePersistent()) {
            assert !TextUtils.isEmpty(snackbar.getActionText())
                    : "Persistent snackbars require action text.";
            mPersistentSnackbars.addFirst(snackbar);
        } else if (snackbar.isTypeAction()) {
            if (getCurrent() != null
                    && !getCurrent().isTypeAction()
                    && !getCurrent().isHighPriority()) {
                removeCurrent(DismissalReason.REPLACED_BY_ACTION_SNACKBAR);
            }
            mSnackbars.addFirst(snackbar);
        } else {
            mSnackbars.addLast(snackbar);
        }

        // Enforce a hard capacity limit to prevent queue exhaustion.
        // Drop the oldest unseen standard notifications from the front of the queue.
        while (mSnackbars.size() > MAX_SNACKBARS) {
            removeOldestUnseenSnackbar(mSnackbars);
        }
        while (mPersistentSnackbars.size() > MAX_SNACKBARS) {
            removeOldestUnseenSnackbar(mPersistentSnackbars);
        }
    }

    private void setHighPrioritySlot(Snackbar snackbar) {
        if (snackbar.isTypePersistent()) {
            mActivePersistentHighPriority = snackbar;
        } else {
            mActiveTransientHighPriority = snackbar;
        }
    }

    private void removeOldestUnseenSnackbar(Deque<Snackbar> list) {
        Iterator<Snackbar> iter = list.iterator();
        if (iter.hasNext() && list.peekFirst() == getCurrent()) {
            iter.next(); // Skip the currently showing one
        }
        if (!iter.hasNext()) return;
        Snackbar toRemove = iter.next();
        iter.remove();
        RecordHistogram.recordEnumeratedHistogram(
                "Snackbar.DismissalReason", DismissalReason.OTHERS, DismissalReason.NUM_ENTRIES);
        SnackbarController controller = toRemove.getController();
        if (controller != null) {
            controller.onDismissNoAction(toRemove.getActionData());
        }
    }

    boolean contains(Snackbar snackbar) {
        return snackbar == mActiveTransientHighPriority
                || snackbar == mActivePersistentHighPriority
                || mSnackbars.contains(snackbar)
                || mPersistentSnackbars.contains(snackbar);
    }

    @Nullable Snackbar getCurrent() {
        if (mActiveTransientHighPriority != null) return mActiveTransientHighPriority;
        if (mActivePersistentHighPriority != null) return mActivePersistentHighPriority;
        Snackbar actionCurrent = mSnackbars.peekFirst();
        Snackbar persistentCurrent = mPersistentSnackbars.peekFirst();
        return actionCurrent != null ? actionCurrent : persistentCurrent;
    }

    @Nullable Snackbar removeCurrentDueToAction() {
        return removeCurrent(DismissalReason.ACTION_BUTTON);
    }

    @Nullable Snackbar removeCurrentDueToSwipe() {
        return removeCurrent(DismissalReason.SWIPE);
    }

    private @Nullable Snackbar removeCurrent(@DismissalReason int reason) {
        Snackbar current = getCurrent();
        if (current == null) return null;

        if (current == mActiveTransientHighPriority) {
            mActiveTransientHighPriority = null;
        } else if (current == mActivePersistentHighPriority) {
            mActivePersistentHighPriority = null;
        } else if (!mSnackbars.isEmpty() && mSnackbars.peekFirst() == current) {
            mSnackbars.pollFirst();
        } else {
            mPersistentSnackbars.pollFirst();
        }

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
        return current;
    }

    boolean isEmpty() {
        return mActiveTransientHighPriority == null
                && mActivePersistentHighPriority == null
                && mSnackbars.isEmpty()
                && mPersistentSnackbars.isEmpty();
    }

    void clear() {
        while (!isEmpty()) {
            removeCurrent(DismissalReason.OTHERS);
        }
    }

    void removeCurrentDueToTimeout() {
        Snackbar current = getCurrent();
        if (current == null || current.isTypePersistent()) {
            return;
        }
        removeCurrent(DismissalReason.TIMEOUT);
    }

    private boolean dismissHighPriorityIfMatching(
            @Nullable Snackbar hp,
            SnackbarController controller,
            @Nullable Object actionData,
            boolean matchActionData) {
        if (hp == null || hp.getController() != controller) return false;
        if (matchActionData && !Objects.equals(hp.getActionData(), actionData)) return false;
        RecordHistogram.recordEnumeratedHistogram(
                "Snackbar.DismissalReason",
                DismissalReason.DISMISSED_BY_CALLER,
                DismissalReason.NUM_ENTRIES);
        controller.onDismissNoAction(hp.getActionData());
        return true;
    }

    boolean removeMatchingSnackbars(SnackbarController controller) {
        boolean removed = false;
        if (dismissHighPriorityIfMatching(mActiveTransientHighPriority, controller, null, false)) {
            mActiveTransientHighPriority = null;
            removed = true;
        }
        if (dismissHighPriorityIfMatching(mActivePersistentHighPriority, controller, null, false)) {
            mActivePersistentHighPriority = null;
            removed = true;
        }
        removed |=
                removeSnackbarFromList(mSnackbars, controller, DismissalReason.DISMISSED_BY_CALLER);
        removed |=
                removeSnackbarFromList(
                        mPersistentSnackbars, controller, DismissalReason.DISMISSED_BY_CALLER);
        return removed;
    }

    private static boolean removeSnackbarFromList(
            Deque<Snackbar> list, SnackbarController controller, @DismissalReason int reason) {
        if (controller == null) return false;
        List<Snackbar> removedSnackbars = new ArrayList<>();
        Iterator<Snackbar> iter = list.iterator();
        while (iter.hasNext()) {
            Snackbar snackbar = iter.next();
            if (snackbar.getController() == controller) {
                iter.remove();
                removedSnackbars.add(snackbar);
            }
        }
        for (Snackbar snackbar : removedSnackbars) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Snackbar.DismissalReason", reason, DismissalReason.NUM_ENTRIES);
            controller.onDismissNoAction(snackbar.getActionData());
        }
        return !removedSnackbars.isEmpty();
    }

    boolean removeMatchingSnackbars(SnackbarController controller, Object data) {
        boolean removed = false;
        if (dismissHighPriorityIfMatching(mActiveTransientHighPriority, controller, data, true)) {
            mActiveTransientHighPriority = null;
            removed = true;
        }
        if (dismissHighPriorityIfMatching(mActivePersistentHighPriority, controller, data, true)) {
            mActivePersistentHighPriority = null;
            removed = true;
        }
        removed |=
                removeSnackbarFromList(
                        mSnackbars, controller, data, DismissalReason.DISMISSED_BY_CALLER);
        removed |=
                removeSnackbarFromList(
                        mPersistentSnackbars,
                        controller,
                        data,
                        DismissalReason.DISMISSED_BY_CALLER);
        return removed;
    }

    private static boolean removeSnackbarFromList(
            Deque<Snackbar> list,
            SnackbarController controller,
            Object data,
            @DismissalReason int reason) {
        if (controller == null) return false;
        List<Snackbar> removedSnackbars = new ArrayList<>();
        Iterator<Snackbar> iter = list.iterator();
        while (iter.hasNext()) {
            Snackbar snackbar = iter.next();
            if (snackbar.getController() == controller
                    && Objects.equals(snackbar.getActionData(), data)) {
                iter.remove();
                removedSnackbars.add(snackbar);
            }
        }
        for (Snackbar snackbar : removedSnackbars) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Snackbar.DismissalReason", reason, DismissalReason.NUM_ENTRIES);
            controller.onDismissNoAction(assumeNonNull(snackbar.getActionData()));
        }
        return !removedSnackbars.isEmpty();
    }
}
