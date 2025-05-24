// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.build.annotations.NullMarked;

/**
 * Specialization of the SimpleSelectionController, that works with a fixed, known set of elements.
 */
@NullMarked
public class SimpleSelectionController extends SelectionController {
    private final OnSelectionChangedListener mListener;
    private int mItemCount;

    @FunctionalInterface
    public interface OnSelectionChangedListener {
        /**
         * Invoked whenever selected state at specific position changed.
         *
         * @param position the position to apply selection change to
         * @param isSelected whether that position should be selected
         */
        void onSelectionChanged(int position, boolean isSelected);
    }

    /**
     * @param listener the listener receiving notifications about selection changes
     * @param itemCount the number of items
     * @param mode Selection mode that defines how the controller will behave
     */
    public SimpleSelectionController(
            OnSelectionChangedListener listener, int itemCount, @Mode int mode) {
        super(mode);

        assert itemCount < Integer.MAX_VALUE;
        assert itemCount >= 0;

        mListener = listener;
        mItemCount = itemCount;
        // Note: this will calculate correct position if there are no items.
        setItemCount(itemCount);
    }

    /** Returns the number of items in the container controlled by the SelectionController. */
    @Override
    protected int getItemCount() {
        return mItemCount;
    }

    /** Update the number of items SelectionController can choose from. */
    public void setItemCount(int newItemCount) {
        mItemCount = newItemCount;
        // Reset position if out of range.
        setPosition(getPosition().orElse(mDefaultPosition));
    }

    @Override
    protected void setItemState(int position, boolean isSelected) {
        mListener.onSelectionChanged(position, isSelected);
    }
}
