// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView.Adapter;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.build.annotations.NullMarked;

/** A data adapter for the Photo Picker. */
@NullMarked
public class PickerAdapter extends Adapter<ViewHolder> {
    // The category view to use to show the images.
    private final PickerCategoryView mCategoryView;

    /**
     * The PickerAdapter constructor.
     *
     * @param categoryView The category view to use to show the images.
     */
    public PickerAdapter(PickerCategoryView categoryView) {
        mCategoryView = categoryView;
    }

    // RecyclerView.Adapter:

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View itemView =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.photo_picker_bitmap_view, parent, false);
        PickerBitmapView bitmapView = (PickerBitmapView) itemView;
        bitmapView.setCategoryView(mCategoryView);
        return new PickerBitmapViewHolder(bitmapView);
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        if (holder instanceof PickerBitmapViewHolder) {
            PickerBitmapViewHolder myHolder = (PickerBitmapViewHolder) holder;
            myHolder.displayItem(mCategoryView, position);
        }
    }

    @Override
    public int getItemCount() {
        assumeNonNull(mCategoryView.getPickerBitmaps());
        return mCategoryView.getPickerBitmaps().size();
    }
}
