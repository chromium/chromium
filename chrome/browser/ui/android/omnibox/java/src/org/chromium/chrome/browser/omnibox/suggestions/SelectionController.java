// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.IntDef;

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
     * Operational modes of the SelectionController.
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
     *   <li>WRAPPING:
     *       <ul>
     *         <li>forward: A -> B -> C -> A -> B
     *         <li>backward: C -> B -> A -> C -> B
     *       </ul>
     *   <li>WRAPPING_WITH_SENTINEL:
     *       <ul>
     *         <li>forward: ∅- -> A -> B -> C -> ∅- -> A
     *         <li>backward: ∅- -> C -> B -> A -> ∅- -> C
     *       </ul>
     *   <li>SENTINEL_THEN_WRAPPING:
     *       <ul>
     *         <li>forward: ∅- -> A -> B -> C -> A -> B
     *         <li>backward: ∅- -> C -> B -> A -> C -> B
     *       </ul>
     * </ul>
     */
    @IntDef({
        TraversalMode.SATURATING,
        TraversalMode.SATURATING_WITH_SENTINEL,
        TraversalMode.WRAPPING,
        TraversalMode.WRAPPING_WITH_SENTINEL,
        TraversalMode.SENTINEL_THEN_WRAPPING
    })
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    public @interface TraversalMode {
        int SATURATING = 0;
        int SATURATING_WITH_SENTINEL = 1;
        int WRAPPING = 2;
        int WRAPPING_WITH_SENTINEL = 3;
        int SENTINEL_THEN_WRAPPING = 4;
    }

    protected @TraversalMode int mMode;
    protected int mDefaultPosition;

    private int mPosition;

    /**
     * SelectionController constructor.
     *
     * @param mode Selection mode that defines how the controller will behave
     */
    public SelectionController(@TraversalMode int mode) {
        mPosition = Integer.MIN_VALUE;
        setSelectionMode(mode);
    }

    /** Sets the selection mode that defines how the controller will behave. */
    public void setSelectionMode(@TraversalMode int mode) {
        mMode = mode;
        switch (mode) {
            case TraversalMode.SATURATING:
            case TraversalMode.WRAPPING:
                mDefaultPosition = 0;
                break;

            case TraversalMode.SATURATING_WITH_SENTINEL:
            case TraversalMode.WRAPPING_WITH_SENTINEL:
            case TraversalMode.SENTINEL_THEN_WRAPPING:
            default:
                mDefaultPosition = Integer.MIN_VALUE; // Lower-end sentinel.
                break;
        }
    }

    /** Resets the controller, making the current position point to default item. */
    public void reset() {
        setPosition(mDefaultPosition);
    }

    public void selectFirstItem() {
        setPosition(0);
    }

    public void selectLastItem() {
        setPosition(getItemCount() - 1);
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
        int itemCount = getItemCount();
        if (itemCount == 0) return false;

        Integer position = getPosition();
        int newPosition = (position == null ? -1 : position) + 1;

        if (mMode == TraversalMode.SATURATING || mMode == TraversalMode.SATURATING_WITH_SENTINEL) {
            if (mPosition == Integer.MAX_VALUE) return false;

            while (newPosition < itemCount) {
                if (isSelectableItem(newPosition)) {
                    return setPosition(newPosition);
                }
                newPosition++;
            }

            if (mMode == TraversalMode.SATURATING_WITH_SENTINEL) {
                setPosition(Integer.MAX_VALUE);
            }
            return false;
        } else if (mMode == TraversalMode.WRAPPING
                || mMode == TraversalMode.SENTINEL_THEN_WRAPPING) {
            // Check full list once, with wrapping, to find the next selectable item.
            for (int i = 0; i < itemCount; i++) {
                if (newPosition >= itemCount) {
                    // Wrap around to the beginning of the list.
                    newPosition -= itemCount;
                }
                if (isSelectableItem(newPosition)) {
                    return setPosition(newPosition);
                }
                newPosition++;
            }
            return false;
        } else if (mMode == TraversalMode.WRAPPING_WITH_SENTINEL) {
            while (newPosition < itemCount) {
                if (isSelectableItem(newPosition)) {
                    return setPosition(newPosition);
                }
                newPosition++;
            }
            // WRAPPING_WITH_SENTINEL supports only one sentinel state Integer.MIN_VALUE.
            setPosition(Integer.MIN_VALUE);
            return false;
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
        int itemCount = getItemCount();
        if (itemCount == 0) return false;

        Integer position = getPosition();
        int newPosition = (position == null ? itemCount : position) - 1;

        if (mMode == TraversalMode.SATURATING || mMode == TraversalMode.SATURATING_WITH_SENTINEL) {
            if (mPosition == Integer.MIN_VALUE) return false;

            while (newPosition >= 0) {
                if (isSelectableItem(newPosition)) {
                    return setPosition(newPosition);
                }
                newPosition--;
            }

            if (mMode == TraversalMode.SATURATING_WITH_SENTINEL) {
                setPosition(Integer.MIN_VALUE);
            }
            return false;
        } else if (mMode == TraversalMode.WRAPPING
                || mMode == TraversalMode.SENTINEL_THEN_WRAPPING) {
            // Check full list once, with wrapping, to find the previous selectable item.
            for (int i = 0; i < itemCount; i++) {
                if (newPosition < 0) {
                    // Wrap around to the end of the list.
                    newPosition += itemCount;
                }
                if (isSelectableItem(newPosition)) {
                    return setPosition(newPosition);
                }
                newPosition--;
            }
            return false;
        } else if (mMode == TraversalMode.WRAPPING_WITH_SENTINEL) {
            while (newPosition >= 0) {
                if (isSelectableItem(newPosition)) {
                    return setPosition(newPosition);
                }
                newPosition--;
            }

            setPosition(Integer.MIN_VALUE);
            return false;
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
     * Set the new counter value, saturating it according to @TraversalMode.
     *
     * @param newPosition - new value to apply to the mPosition
     * @return whether selection was applied to the new element.
     */
    protected boolean setPosition(int newPosition) {
        // Compute new position.
        int itemCount = getItemCount();
        switch (mMode) {
            case TraversalMode.SATURATING:
            case TraversalMode.WRAPPING:
                if (itemCount == 0) {
                    newPosition = Integer.MIN_VALUE;
                } else {
                    newPosition = MathUtils.clamp(newPosition, 0, itemCount - 1);
                }
                break;

            case TraversalMode.SATURATING_WITH_SENTINEL:
                // Park outside the valid range, keeping the information which edge we hit.
                if (newPosition < 0) { // Underflow
                    newPosition = Integer.MIN_VALUE;
                } else if (newPosition >= itemCount) {
                    newPosition = Integer.MAX_VALUE;
                }
                break;

            case TraversalMode.WRAPPING_WITH_SENTINEL:
                if (newPosition < 0 || newPosition >= itemCount) {
                    newPosition = Integer.MIN_VALUE;
                }
                break;

            case TraversalMode.SENTINEL_THEN_WRAPPING:
                if (newPosition == Integer.MIN_VALUE || itemCount == 0) {
                    newPosition = Integer.MIN_VALUE;
                } else {
                    newPosition = MathUtils.clamp(newPosition, 0, itemCount - 1);
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
     * @param isSelected Whether the suggestion item view is currently selected.
     */
    protected abstract void setItemState(int position, boolean isSelected);
}
