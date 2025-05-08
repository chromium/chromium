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

/**
 * Helper class allowing advancing forward/backward while saturating outside the valid range.
 *
 * <p>TODO(344930378): Explore possibility to reconcile this with RecyclerViewSelectionController.
 * The two classes serve similar purpose, but the complexity of view recycling may make the merge
 * difficult. This controller expands RVSC capabilities, however the following aspects make
 * immediate merge difficult: - volume of items changing at runtime, - exposure triggering (to
 * ensure we can locate views for items not currently bound), - reused views propagate the selected
 * state when rebound to a different item,
 *
 * <p>Consider adding WRAPPING and WRAPPING_WITH_SENTINEL variants to allow cycling through.
 */
@NullMarked
public class SelectionController {
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

    private final OnSelectionChangedListener mListener;
    private final @Mode int mMode;
    private final int mDefaultPosition;
    private int mMaxPosition;
    private int mPosition;

    @FunctionalInterface
    public interface OnSelectionChangedListener {
        /**
         * Invoked whenever selected state at specific position changed.
         *
         * @param position the position to apply selection change to
         * @param isSelected whether that position should be selected
         * @return whether selection was applied at requested position
         */
        boolean onSelectionChanged(int position, boolean isSelected);
    }

    /**
     * SelectionController constructor.
     *
     * @param listener the listener receiving notifications about selection changes
     */
    public SelectionController(OnSelectionChangedListener listener, @Mode int mode) {
        this(listener, 0, mode);
    }

    /**
     * SelectionController constructor.
     *
     * @param listener the listener receiving notifications about selection changes
     * @param maxPosition the maximum valid position that can be reported to the listener
     * @param mode Selection mode that defines how the controller will behave
     */
    public SelectionController(
            OnSelectionChangedListener listener, int maxPosition, @Mode int mode) {
        assert maxPosition < Integer.MAX_VALUE;
        assert maxPosition >= 0;

        switch (mode) {
            case Mode.SATURATING:
                mDefaultPosition = 0;
                break;

            case Mode.SATURATING_WITH_SENTINEL:
            default:
                mDefaultPosition = -1; // Just before the first entry.
                break;
        }

        // Initialization step only, to ensure we do not emit bogus selection change event.
        mPosition = Integer.MIN_VALUE;
        mListener = listener;
        mMode = mode;
        updateMaxPosition(maxPosition);
    }

    /**
     * Update range of valid positions.
     *
     * @param maxPosition the upper value in the selection range (inclusive)
     */
    public void updateMaxPosition(int maxPosition) {
        if (!isParkedAtSentinel()) {
            mListener.onSelectionChanged(mPosition, false);
        }

        mMaxPosition = maxPosition;
        mPosition = mDefaultPosition;

        if (!isParkedAtSentinel()) {
            mListener.onSelectionChanged(mPosition, true);
        }
    }

    /** Resets the controller, making the current position point to default item. */
    public void reset() {
        setPosition(mDefaultPosition);
    }

    /**
     * Advances the counter towards the maxPosition, returning false if the held value has
     * saturated.
     *
     * @return whether selection was applied to the new element.
     */
    public boolean advanceForward() {
        return setPosition(mPosition + 1);
    }

    /**
     * Advances the counter towards the minPosition, returning false if the held value has
     * saturated.
     *
     * @return whether selection was applied to the new element.
     */
    public boolean advanceBack() {
        return setPosition(mPosition - 1);
    }

    /** Returns true if selection controller is currently parked outside the valid range. */
    public boolean isParkedAtSentinel() {
        return mPosition < 0 || mPosition > mMaxPosition;
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
            mListener.onSelectionChanged(mPosition, false);
        }

        int oldPosition = mPosition;
        mPosition = newPosition;
        switch (mMode) {
            case Mode.SATURATING:
                mPosition = MathUtils.clamp(mPosition, 0, mMaxPosition);
                break;

            case Mode.SATURATING_WITH_SENTINEL:
                // Park outside the valid range, keeping the information which edge we hit.
                mPosition = MathUtils.clamp(mPosition, -1, mMaxPosition + 1);
                break;
        }

        if (isParkedAtSentinel()) return false;

        // Select new item, fall back to old position if not possible.
        if (!mListener.onSelectionChanged(mPosition, true)) {
            mPosition = oldPosition;
            mListener.onSelectionChanged(mPosition, true);
            // We failed to select the requested entry.
            return false;
        }

        return true;
    }
}
