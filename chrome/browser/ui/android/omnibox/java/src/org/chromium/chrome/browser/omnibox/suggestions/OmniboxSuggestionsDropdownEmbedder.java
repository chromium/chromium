// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.annotation.SuppressLint;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;

import java.util.Objects;

/** Provider of capabilities required to embed the omnibox suggestion list into the UI. */
public interface OmniboxSuggestionsDropdownEmbedder {

    /**
     * POD type that encapsulates the "alignment" (position, width, padding) of the omnibox
     * dropdown.
     */
    class OmniboxAlignment {

        public static final OmniboxAlignment UNSPECIFIED =
                new OmniboxAlignment(-1, -1, -1, -1, -1, -1, -1);
        public final int left;
        public final int top;
        public final int width;
        public final int height;
        public final int paddingLeft;
        public final int paddingRight;
        public final int paddingBottom;

        public OmniboxAlignment(
                int left,
                int top,
                int width,
                int height,
                int paddingLeft,
                int paddingRight,
                int paddingBottom) {
            this.left = left;
            this.top = top;
            this.width = width;
            this.paddingLeft = paddingLeft;
            this.paddingRight = paddingRight;
            this.paddingBottom = paddingBottom;
            this.height = height;
        }

        @Override
        public int hashCode() {
            return Objects.hash(left, top, width, paddingLeft, paddingRight, paddingBottom);
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (!(obj instanceof OmniboxAlignment)) return false;
            OmniboxAlignment other = (OmniboxAlignment) obj;
            return other.left == this.left
                    && other.top == this.top
                    && other.width == this.width
                    && other.height == this.height
                    && other.paddingLeft == this.paddingLeft
                    && other.paddingRight == this.paddingRight
                    && other.paddingBottom == this.paddingBottom;
        }

        @SuppressLint("DefaultLocale")
        @NonNull
        @Override
        public String toString() {
            return String.format(
                    "OmniboxAlignment left: %d top: %d width: %d height: %d paddingLeft: %d"
                            + " paddingRight: %d paddingBottom: %d",
                    left, top, width, height, paddingLeft, paddingRight, paddingBottom);
        }

        /**
         * Returns whether the difference from the given alignment object is solely in terms of left
         * or padding.
         */
        public boolean isOnlyHorizontalDifference(@Nullable OmniboxAlignment other) {
            if (other == null) return false;
            return (this.left != other.left
                            || (this.paddingLeft != other.paddingLeft
                                    && this.paddingRight != other.paddingRight))
                    && (this.top == other.top
                            && this.width == other.width
                            && this.paddingBottom == other.paddingBottom);
        }

        /**
         * Returns whether the width of the given OmniboxAlignment differs from this alignment
         * object's width. Returns true for a null argument.
         */
        public boolean doesWidthDiffer(@Nullable OmniboxAlignment other) {
            if (other == null) return true;
            return this.width != other.width;
        }
    }

    /**
     * Adds an observer of alignment changes. The supplied callback will be invoked when alignment
     * is recalculated.
     */
    OmniboxAlignment addAlignmentObserver(Callback<OmniboxAlignment> obs);

    /**
     * Remove an observer of alignment changes. The supplied callback will be no longer invoked when
     * alignment is recalculated.
     */
    void removeAlignmentObserver(Callback<OmniboxAlignment> obs);

    /**
     * Returns the current alignment values, but does not recalculate them. Will not return null but
     * may return {@link OmniboxAlignment.UNSPECIFIED} if there is not a currently valid alignment.
     */
    @NonNull
    OmniboxAlignment getCurrentAlignment();

    /** Return whether the suggestions are being rendered in the tablet UI. */
    boolean isTablet();

    /**
     * The dropdown must call this when it is attached to the window to start the process of
     * alignment recalculation. Updates are skipped prior to this point to avoid repeated, unused
     * calculation of alignment values.
     */
    void onAttachedToWindow();

    /**
     * The dropdown should call this when it is detached from the window to pause the process of
     * alignment recalculation.
     */
    void onDetachedFromWindow();

    /** The vertical translation that should be used during focus animations. */
    float getVerticalTranslationForAnimation();
}
