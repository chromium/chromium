// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Helper class allowing advancing forward/backward while saturating outside the valid range. */
@NullMarked
public abstract class SelectionController {
    /**
     * Operational modes of the SelectionController
     *
     * <ul>
     *   <li>SATURATING:
     *       <ul>
     *         <li>forward: A -> B -> C -> C -> C
     *         <li>backward: C -> B -> A -> A -> A
     *       </ul>
     *   <li>SATURATING_WITH_SENTINEL:
     *       <ul>
     *         <li>forward: ∅- -> A -> B -> C -> ∅+ -> ∅+
     *         <li>backward: ∅+ -> C -> B -> A -> ∅- -> ∅-
     *       </ul>
     * </ul>
     */
    @IntDef({Mode.SATURATING, Mode.SATURATING_WITH_SENTINEL})
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    public @interface Mode {
        int SATURATING = 0;
        int SATURATING_WITH_SENTINEL = 1;
    }

    protected final @Mode int mMode;
    protected final int mDefaultPosition;

    private int mPosition;

    /**
     * SelectionController constructor.
     *
     * @param mode Selection mode that defines how the controller will behave
     */
    public SelectionController(@Mode int mode) {
        switch (mode) {
            case Mode.SATURATING:
                mDefaultPosition = 0;
                break;

            case Mode.SATURATING_WITH_SENTINEL:
            default:
                mDefaultPosition = Integer.MIN_VALUE; // Lower-end sentinel.
                break;
        }

        mPosition = Integer.MIN_VALUE;
        mMode = mode;
    }

    /** Resets the controller, making the current position point to default item. */
    public void reset() {
        setPosition(mDefaultPosition);
    }

    /** Returns the maximum valid position the SelectionController can assume. */
    protected abstract int getItemCount();

    /**
     * Advances the counter towards the maxPosition, returning false if the held value has
     * saturated.
     *
     * @return whether selection was applied to the new element.
     */
    public boolean selectNextItem() {
        // If parked at upper sentinel, bail.
        if (mPosition == Integer.MAX_VALUE) return false;

        // If parked at lower sentinel, resume from 0.
        Integer position = getPosition();
        int newPosition = (position == null ? -1 : position) + 1;
        int itemCount = getItemCount();
        while (newPosition < itemCount) {
            if (isSelectableItem(newPosition)) {
                return setPosition(newPosition);
            }
            newPosition++;
        }

        // Don't touch selection if we can't advance. Otherwise, park at sentinel.
        if (mMode == Mode.SATURATING_WITH_SENTINEL) {
            setPosition(Integer.MAX_VALUE);
        }
        return false;
    }

    /**
     * Advances the counter towards the minPosition, returning false if the held value has
     * saturated.
     *
     * @return whether selection was applied to the new element.
     */
    public boolean selectPreviousItem() {
        // If parked at lower sentinel, bail.
        if (mPosition == Integer.MIN_VALUE) return false;

        // If parked at upper sentinel, resume from getItemCount() - 1.
        Integer position = getPosition();
        int newPosition = (position == null ? getItemCount() : position) - 1;
        while (newPosition >= 0) {
            if (isSelectableItem(newPosition)) {
                return setPosition(newPosition);
            }
            newPosition--;
        }

        // Don't touch selection if we can't advance. Otherwise, park at sentinel.
        if (mMode == Mode.SATURATING_WITH_SENTINEL) {
            setPosition(Integer.MIN_VALUE);
        }
        return false;
    }

    /** Returns whether specific position is a sentinel. */
    private static boolean isSentinel(int position) {
        return position == Integer.MIN_VALUE || position == Integer.MAX_VALUE;
    }

    /** Returns true if selection controller is currently parked outside the valid range. */
    public boolean isParkedAtSentinel() {
        return isSentinel(mPosition);
    }

    /** Returns current counter value (unless saturated). */
    public @Nullable Integer getPosition() {
        if (isParkedAtSentinel()) return null;
        return mPosition;
    }

    /**
     * Set the new counter value, saturating it according to @Mode.
     *
     * @param newPosition - new value to apply to the mPosition
     * @return whether selection was applied to the new element.
     */
    @VisibleForTesting
    boolean setPosition(int newPosition) {
        // Compute new position.
        int itemCount = getItemCount();
        switch (mMode) {
            case Mode.SATURATING:
                if (itemCount == 0) {
                    newPosition = Integer.MIN_VALUE;
                } else {
                    newPosition = MathUtils.clamp(newPosition, 0, itemCount - 1);
                }
                break;

            case Mode.SATURATING_WITH_SENTINEL:
                // Park outside the valid range, keeping the information which edge we hit.
                if (newPosition < 0) { // Underflow
                    newPosition = Integer.MIN_VALUE;
                } else if (newPosition >= itemCount) {
                    newPosition = Integer.MAX_VALUE;
                }
                break;
        }

        // Do not attempt to move selection if the next item is not selectable.
        if (!isSentinel(newPosition) && !isSelectableItem(newPosition)) {
            return false;
        }

        if (!isParkedAtSentinel()) {
            setItemState(mPosition, false);
        }

        mPosition = newPosition;

        if (!isParkedAtSentinel()) {
            setItemState(mPosition, true);
            return true;
        }

        return false;
    }

    /** Returns whether view at specific position is focusable. */
    protected boolean isSelectableItem(int position) {
        return true;
    }

    /**
     * Applies selection change at specific position.
     *
     * @param position the index of an element to change the state of
     * @param state the desired new state
     */
    protected abstract void setItemState(int position, boolean isSelected);
}
