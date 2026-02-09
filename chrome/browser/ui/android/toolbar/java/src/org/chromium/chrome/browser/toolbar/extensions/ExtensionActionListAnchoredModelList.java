// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionButtonProperties.ListItemType;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;

/**
 * A wrapper around {@link ModelList} that adds two non-visible placeholder items, one at the
 * beginning and one at the end of the list. These "anchors" are used to ensure toolbar action
 * animations work correctly.
 *
 * <p>When an icon at the edge of the list becomes visible or invisible, {@link RecyclerView} it
 * uses a fading animation. However, we want icons to slide. By adding these placeholder anchors,
 * the extension icons are never at the edge of the {@link RecyclerView}, which allows for the
 * desired sliding animation.
 *
 * <p><b>List Structure:</b>
 *
 * <pre>
 * [0]   Start Anchor (Placeholder)
 * [1]   Action A
 * [2]   Action B
 * [n-1] Action Z
 * [n]   End Anchor (Placeholder)
 * </pre>
 *
 * <p>Modifications to the list of extension actions should be made to the source {@link ModelList},
 * not this one.
 */
@NullMarked
public class ExtensionActionListAnchoredModelList extends ModelList implements Destroyable {

    private final ModelList mSourceList;

    // We override all methods in {@link ListObserver} because we have to offset everything by 1
    // due to the start anchor.
    private final ListObserver<Void> mSourceObserver =
            new ListObserver<Void>() {
                @Override
                public void onItemRangeInserted(ListObservable source, int index, int count) {
                    // Collect the new items into a temporary list to use {@code addAll()}.
                    ArrayList<ListItem> newItems = new ArrayList<>(count);
                    for (int i = 0; i < count; i++) {
                        newItems.add(mSourceList.get(index + i));
                    }
                    ExtensionActionListAnchoredModelList.super.addAll(newItems, index + 1);
                }

                @Override
                public void onItemRangeRemoved(ListObservable source, int index, int count) {
                    ExtensionActionListAnchoredModelList.super.removeRange(index + 1, count);
                }

                @Override
                public void onItemRangeChanged(
                        ListObservable source, int index, int count, @Nullable Void payload) {
                    for (int i = 0; i < count; i++) {
                        ExtensionActionListAnchoredModelList.super.update(
                                index + i + 1, mSourceList.get(index + i));
                    }
                }

                @Override
                public void onItemMoved(ListObservable source, int curIndex, int newIndex) {
                    ExtensionActionListAnchoredModelList.super.move(curIndex + 1, newIndex + 1);
                }
            };

    public ExtensionActionListAnchoredModelList(ModelList sourceList) {
        mSourceList = sourceList;

        // Add the start anchor (placeholder) at index 0.
        super.add(
                new ListItem(
                        ListItemType.ANCHOR,
                        new PropertyModel.Builder(ExtensionActionButtonProperties.ALL_KEYS)
                                .build()));

        // Populate the existing items from the source.
        for (int i = 0; i < mSourceList.size(); i++) {
            super.add(mSourceList.get(i));
        }

        // Add the end anchor (placeholder) at the very end.
        super.add(
                new ListItem(
                        ListItemType.ANCHOR,
                        new PropertyModel.Builder(ExtensionActionButtonProperties.ALL_KEYS)
                                .build()));

        mSourceList.addObserver(mSourceObserver);
    }

    @Override
    public void destroy() {
        mSourceList.removeObserver(mSourceObserver);
    }

    /**
     * Drag directly modifies {@link this}, so we have to propagate the change to our source {@link
     * ModelList}.
     */
    @Override
    public void move(int curIndex, int newIndex) {
        super.move(curIndex, newIndex);

        // Remove observer temporarily to avoid recursion.
        mSourceList.removeObserver(mSourceObserver);

        mSourceList.move(curIndex - 1, newIndex - 1);
        mSourceList.addObserver(mSourceObserver);
    }

    /**
     * Returns the adapter position in this wrapper list for the given action ID. This returns the
     * index relative to the RecyclerView (including the Start Anchor), not the index in the
     * underlying source list.
     */
    public int getIndexForActionId(String actionId) {
        for (int i = 1; i < size() - 1; i++) {
            PropertyModel model = get(i).model;
            if (actionId.equals(model.get(ExtensionActionButtonProperties.ID))) {
                return i;
            }
        }
        return RecyclerView.NO_POSITION;
    }
}
