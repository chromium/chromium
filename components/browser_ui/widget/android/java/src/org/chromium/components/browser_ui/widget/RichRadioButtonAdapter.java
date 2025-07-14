// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;

import java.util.List;

/**
 * A RecyclerView adapter for displaying a list of {@link RichRadioButtonData} objects. It manages
 * single-item selection logic across the list and applies internal item orientation based on the
 * overall list layout mode.
 */
@NullMarked
public class RichRadioButtonAdapter
        extends RecyclerView.Adapter<RichRadioButtonAdapter.ViewHolder> {

    /** A listener interface for notifying the host of selected item changes. */
    public interface OnItemSelectedListener {
        void onItemSelected(@NonNull String selectedId);
    }

    private @NonNull List<RichRadioButtonData> mOptions;
    private @Nullable String mSelectedItemId;
    private @RichRadioButtonList.LayoutMode int mCurrentLayoutMode;

    /**
     * Creates a new RichRadioButtonAdapter with the given options and layout mode.
     *
     * @param options The list of options to display.
     * @param layoutMode The layout mode to use.
     */
    public RichRadioButtonAdapter(
            @NonNull List<RichRadioButtonData> options,
            @RichRadioButtonList.LayoutMode int layoutMode) {
        this.mOptions = options;
        this.mCurrentLayoutMode = layoutMode;
    }

    /**
     * Updates the data set of the adapter.
     *
     * @param newOptions The new list of options.
     */
    public void setOptions(@NonNull List<RichRadioButtonData> newOptions) {
        mOptions = newOptions;
    }

    /**
     * Updates the current layout mode of the adapter.
     *
     * @param layoutMode The new layout mode.
     */
    public void setLayoutMode(@RichRadioButtonList.LayoutMode int layoutMode) {
        mCurrentLayoutMode = layoutMode;
    }

    public void setSelectedItem(@NonNull String itemId) {
        // TODO(crbug.com/410752554): Implement selection logic.
    }

    @Override
    public int getItemViewType(int position) {
        return 0;
    }

    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View itemView = new RichRadioButton(parent.getContext());

        itemView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        return new ViewHolder(itemView);
    }

    @Override
    public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
        RichRadioButtonData data = mOptions.get(position);
        RichRadioButton singleButton = (RichRadioButton) holder.itemView;

        singleButton.setChecked(false);

        boolean isInternalVertical =
                (mCurrentLayoutMode == RichRadioButtonList.LayoutMode.TWO_COLUMN_GRID);

        singleButton.setItemData(data.iconResId, data.title, data.description, isInternalVertical);
        singleButton.setChecked(data.id.equals(mSelectedItemId));

        // TODO(crbug.com/410752554): Call setSelection() when the button is clicked.
    }

    @Override
    public int getItemCount() {
        return mOptions.size();
    }

    static class ViewHolder extends RecyclerView.ViewHolder {
        public ViewHolder(@NonNull View itemView) {
            super(itemView);
        }
    }
}
