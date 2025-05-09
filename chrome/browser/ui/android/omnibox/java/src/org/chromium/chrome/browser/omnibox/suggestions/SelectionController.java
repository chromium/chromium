// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.OptionalInt;

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
    public boolean advanceForward() {
        if (mPosition == Integer.MAX_VALUE) return false;
        if (mPosition == Integer.MIN_VALUE) return setPosition(0);
        return setPosition(mPosition + 1);
    }

    /**
     * Advances the counter towards the minPosition, returning false if the held value has
     * saturated.
     *
     * @return whether selection was applied to the new element.
     */
    public boolean advanceBack() {
        if (mPosition == Integer.MIN_VALUE) return false;
        if (mPosition == Integer.MAX_VALUE) return setPosition(getItemCount());
        return setPosition(mPosition - 1);
    }

    /** Returns true if selection controller is currently parked outside the valid range. */
    public boolean isParkedAtSentinel() {
        return mPosition == Integer.MIN_VALUE || mPosition == Integer.MAX_VALUE;
    }

    /** Returns current counter value (unless saturated). */
    public OptionalInt getPosition() {
        if (isParkedAtSentinel()) return OptionalInt.empty();
        return OptionalInt.of(mPosition);
    }

    /**
     * Set the new counter value, saturating it according to @Mode.
     *
     * @param newPosition - new value to apply to the mPosition
     * @return whether selection was applied to the new element.
     */
    @VisibleForTesting
    boolean setPosition(int newPosition) {
        if (!isParkedAtSentinel()) {
            setItemState(mPosition, false);
        }

        int oldPosition = mPosition;
        int itemCount = getItemCount();
        mPosition = newPosition;
        switch (mMode) {
            case Mode.SATURATING:
                if (itemCount == 0) {
                    mPosition = Integer.MIN_VALUE;
                } else {
                    mPosition = MathUtils.clamp(mPosition, 0, itemCount - 1);
                }
                break;

            case Mode.SATURATING_WITH_SENTINEL:
                // Park outside the valid range, keeping the information which edge we hit.
                if (mPosition < 0) { // Underflow
                    mPosition = Integer.MIN_VALUE;
                } else if (mPosition >= itemCount) {
                    mPosition = Integer.MAX_VALUE;
                }
                break;
        }

        if (isParkedAtSentinel()) return false;

        // Select new item, fall back to old position if not possible.
        if (!setItemState(mPosition, true)) {
            mPosition = oldPosition;
            setItemState(mPosition, true);
            // We failed to select the requested entry.
            return false;
        }

        return true;
    }

    /**
     * Applies selection change at specific position.
     *
     * @param position the index of an element to change the state of
     * @param state the desired new state
     * @return the applied state of the item at specified position.
     */
    protected abstract boolean setItemState(int position, boolean isSelected);
}
