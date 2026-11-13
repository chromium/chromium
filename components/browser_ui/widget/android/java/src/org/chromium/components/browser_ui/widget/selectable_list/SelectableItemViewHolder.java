// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.selectable_list;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.build.annotations.NullMarked;

/**
 * A ViewHolder for a {@link SelectableItemViewBase}.
 *
 * @param <E> The type of the item associated with the {@link SelectableItemViewBase}.
 */
@NullMarked
public class SelectableItemViewHolder<E> extends ViewHolder {
    private final SelectableItemViewBase<E> mItemView;

    /**
     * @param itemView The {@link SelectableItemViewBase} to be held by this ViewHolder.
     * @param delegate The {@link SelectionDelegate} for the itemView.
     */
    @SuppressWarnings("unchecked")
    public SelectableItemViewHolder(View itemView, SelectionDelegate<E> delegate) {
        super(itemView);
        mItemView = (SelectableItemViewBase<E>) itemView;
        mItemView.setSelectionDelegate(delegate);
    }

    /**
     * @param item The item to display in the {@link SelectableItemViewBase} held by this {@link
     *     ViewHolder}.
     */
    public void displayItem(E item) {
        mItemView.setItem(item);
    }

    /**
     * @return The {@link SelectableItemViewBase} held by this ViewHolder.
     */
    public SelectableItemViewBase<E> getItemView() {
        return mItemView;
    }
}
