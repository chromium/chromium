// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.selectable_list;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/**
 * A generic delegate used to keep track of selected items.
 * @param <E> The type of the selectable items this delegate interacts with.
 */
@NullMarked
public class SelectionDelegate<E> {
    // True if the SelectionDelegate should only support a single item being selected at a time.
    private boolean mIsSingleSelection;

    // Whether selection mode should be enabled all the time (even when 0 items are selected).
    private boolean mEnableSelectionForZeroItems;

    /** Creates a {@link SelectionDelegate}. */
    public SelectionDelegate() {}

    /**
     * Creates a {@link SelectionDelegate} with the given allowing / disallowing being in selection
     * mode when 0 items are selected.
     *
     * @param enableSelectionForZeroItems Whether selection mode should be enabled all the time
     *     (even when 0 items are selected).
     */
    public SelectionDelegate(boolean enableSelectionForZeroItems) {
        mEnableSelectionForZeroItems = enableSelectionForZeroItems;
    }

    /**
     * Observer interface to be notified of selection changes.
     *
     * @param <E> The type of the selectable items this delegate interacts with.
     */
    public interface SelectionObserver<E> {
        /**
         * Called when the set of selected items has changed.
         * @param selectedItems The list of currently selected items. An empty list indicates there
         *                      is no selection.
         */
        void onSelectionStateChange(List<E> selectedItems);
    }

    private Set<E> mSelectedItems = new LinkedHashSet<>();
    private final ObserverList<SelectionObserver<E>> mObservers = new ObserverList<>();

    /** Sets the mode of this SelectionDelegate to single-selection. */
    public void setSingleSelectionMode() {
        mIsSingleSelection = true;
    }

    /**
     * Toggles the selected state for the given item.
     *
     * @param item The item to toggle.
     * @return Whether the item is selected.
     */
    public boolean toggleSelectionForItem(E item) {
        if (mSelectedItems.contains(item)) {
            mSelectedItems.remove(item);
        } else {
            if (mIsSingleSelection) mSelectedItems.clear();
            mSelectedItems.add(item);
        }

        notifyObservers();

        return isItemSelected(item);
    }

    /**
     * Initializes the selected item list with a new set (clears previous selection).
     * @param items The items to set as selected.
     */
    public void setSelectedItems(Set<E> items) {
        mSelectedItems = items;
        notifyObservers();
    }

    /**
     * True if the item is selected. False otherwise.
     * @param item The item.
     * @return Whether the item is selected.
     */
    public boolean isItemSelected(E item) {
        return mSelectedItems.contains(item);
    }

    /**
     * @return Whether selection mode is enabled, allowing the user to select / deselect items.
     */
    public boolean isSelectionEnabled() {
        return !mSelectedItems.isEmpty() || mEnableSelectionForZeroItems;
    }

    /** Clears all selected items. */
    public void clearSelection() {
        mSelectedItems.clear();
        notifyObservers();
    }

    /** @return The set of selected items. */
    public Set<E> getSelectedItems() {
        return mSelectedItems;
    }

    /**
     * @return The list of selected items.
     */
    public List<E> getSelectedItemsAsList() {
        return new ArrayList<>(mSelectedItems);
    }

    /**
     * Adds a SelectionObserver to be notified of selection changes.
     * @param observer The SelectionObserver to add.
     */
    public void addObserver(SelectionObserver<E> observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes a SelectionObserver.
     * @param observer The SelectionObserver to remove.
     */
    public void removeObserver(SelectionObserver<E> observer) {
        mObservers.removeObserver(observer);
    }

    private void notifyObservers() {
        List<E> selectedItems = getSelectedItemsAsList();
        for (SelectionObserver<E> observer : mObservers) {
            observer.onSelectionStateChange(selectedItems);
        }
    }
}
