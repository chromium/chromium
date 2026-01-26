// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.dragreorder.DragTouchHandler;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/** A custom {@link DragTouchHandler} implementation for toolbar actions. */
@NullMarked
public class ExtensionsToolbarDragTouchHandler extends DragTouchHandler {

    public ExtensionsToolbarDragTouchHandler(Context context, ModelList listData) {
        super(context, listData);
    }

    @Override
    public int getMovementFlags(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        int dragFlags = 0;
        if ((getDraggedViewHolder() == viewHolder || getDraggedViewHolder() == null)
                && isActivelyDraggable(viewHolder)) {
            dragFlags = ItemTouchHelper.LEFT | ItemTouchHelper.RIGHT;
        }
        return makeMovementFlags(dragFlags, /* swipeFlags= */ 0);
    }
}
