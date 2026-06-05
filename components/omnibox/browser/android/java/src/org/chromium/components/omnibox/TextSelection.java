// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/**
 * Encapsulates a directed text selection with a starting anchor point ('from') and an active cursor
 * point ('to').
 */
@NullMarked
public final class TextSelection {
    /** The selection range encapsulating all the text. */
    public static final TextSelection SELECT_ALL = new TextSelection(Integer.MAX_VALUE, 0);

    /** The selection range that selects no text, and places the cursor at the end of input. */
    public static final TextSelection SELECT_END =
            new TextSelection(Integer.MAX_VALUE, Integer.MAX_VALUE);

    /** The selection range representing an invalid or reset selection. */
    public static final TextSelection INVALID = new TextSelection(-1, -1);

    /** The starting anchor point of the selection. */
    public final int from;

    /** The active cursor point of the selection. */
    public final int to;

    /**
     * Constructs a new TextSelection.
     *
     * @param from The starting anchor point.
     * @param to The active cursor point.
     */
    public TextSelection(int from, int to) {
        this.from = Math.max(-1, from);
        this.to = Math.max(-1, to);
    }

    /**
     * Clamps 'from' and 'to' bounds to be within [0, maxLength].
     *
     * @param maxLength The maximum length to clamp to.
     * @return A new TextSelection if clamping occurred, or 'this' if no changes were needed.
     */
    public TextSelection trimTo(int maxLength) {
        int clampedFrom = Math.max(0, Math.min(from, maxLength));
        int clampedTo = Math.max(0, Math.min(to, maxLength));
        if (clampedFrom == from && clampedTo == to) {
            return this;
        }
        return new TextSelection(clampedFrom, clampedTo);
    }

    /**
     * Returns the lower bound of the selection (min of 'from' and 'to').
     *
     * @return The lower bound.
     */
    public int getLower() {
        return Math.min(from, to);
    }

    /**
     * Returns the upper bound of the selection (max of 'from' and 'to').
     *
     * @return The upper bound.
     */
    public int getUpper() {
        return Math.max(from, to);
    }

    /**
     * Returns whether the selection is collapsed (empty).
     *
     * @return True if 'from' equals 'to', false otherwise.
     */
    public boolean isCollapsed() {
        return from == to;
    }

    /**
     * Returns whether this selection covers the entire text of the given length.
     *
     * @param length The length of the text.
     * @return True if the selection covers the entire text, false otherwise.
     */
    public boolean selectsAll(int length) {
        return getLower() == 0 && getUpper() >= length;
    }

    @Override
    public boolean equals(@Nullable Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        TextSelection that = (TextSelection) o;
        return from == that.from && to == that.to;
    }

    @Override
    public int hashCode() {
        return Objects.hash(from, to);
    }

    @Override
    public String toString() {
        return "TextSelection{from=" + from + ", to=" + to + "}";
    }
}
