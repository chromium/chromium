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

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

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

    private final @NonNull OnItemSelectedListener mListener;
    private final @NonNull Map<String, Integer> mIdToPositionMap;
    private  final @NonNull List<RichRadioButtonData> mOptions;

    private @Nullable String mSelectedItemId;
    private @RichRadioButtonList.LayoutMode int mCurrentLayoutMode;
    private int mSelectedPosition;

    /**
     * Creates a new RichRadioButtonAdapter with the given options and layout mode.
     *
     * @param options The list of options to display.
     * @param listener The listener for selection changes.
     * @param layoutMode The layout mode to use.
     */
    public RichRadioButtonAdapter(
            @NonNull List<RichRadioButtonData> options,
            @NonNull OnItemSelectedListener listener,
            @RichRadioButtonList.LayoutMode int layoutMode) {
        mOptions = options;
        mListener = listener;
        mCurrentLayoutMode = layoutMode;
        mIdToPositionMap = new HashMap<>();

        initOptions();
    }

    private void initOptions() {

        buildIdToPositionMap();

        if (!mOptions.isEmpty()) {
            selectFirstItemAsDefault();
        }
    }

    private void selectFirstItemAsDefault() {
        if (mSelectedItemId == null && !mOptions.isEmpty()) {
            setSelection(0, mOptions.get(0).id);
        }
    }

    /**
     * Updates the current layout mode of the adapter.
     *
     * @param layoutMode The new layout mode.
     */
    public void setLayoutMode(@RichRadioButtonList.LayoutMode int layoutMode) {
        mCurrentLayoutMode = layoutMode;
    }

    /**
     * Builds or rebuilds the ID to position map. This must be called whenever `mOptions` changes to
     * ensure the map is up-to-date.
     */
    private void buildIdToPositionMap() {
        mIdToPositionMap.clear();
        for (int i = 0; i < mOptions.size(); i++) {
            mIdToPositionMap.put(mOptions.get(i).id, i);
        }
    }

    /**
     * Sets the selected item by its ID.
     *
     * <p>If the {@code itemId} is found: the item at that ID is selected. If the {@code itemId} is
     * NOT found: - If no item is currently selected and the options list is not empty, the first
     * item in the list is selected by default. - Otherwise (an item is already selected, or the
     * options list is empty), no change occurs, and the selection remains as is.
     *
     * <p>The `onItemSelected` listener is only notified if the selection state genuinely changes.
     *
     * @param itemId The ID of the item to select.
     */
    public void setSelectedItem(@NonNull String itemId) {
        Integer newPositionWrapper = mIdToPositionMap.get(itemId);

        assert newPositionWrapper != null
                : "Attempted to select an item with ID "
                        + itemId
                        + " that is not in the options list.";

        setSelection(newPositionWrapper, itemId);
    }

    /**
     * Internal method to manage selection state and notify UI/listener. This method ensures
     * `onItemSelected` is called ONLY if the selected item ID changes.
     *
     * @param newPosition The new selected position.
     * @param newSelectedItemId The new newSelectedItemId (can be null for no selection).
     */
    private void setSelection(int newPosition, @Nullable String newSelectedItemId) {
        @Nullable String oldSelectedItemIdBeforeUpdate = mSelectedItemId;
        int oldSelectedPositionBeforeUpdate = mSelectedPosition;

        if (newPosition == oldSelectedPositionBeforeUpdate
                && Objects.equals(newSelectedItemId, oldSelectedItemIdBeforeUpdate)) {
            return;
        }

        mSelectedItemId = newSelectedItemId;
        mSelectedPosition = newPosition;

        notifyItemChanged(oldSelectedPositionBeforeUpdate);
        notifyItemChanged(mSelectedPosition);

        if (mSelectedItemId != null) {
            mListener.onItemSelected(mSelectedItemId);
        }
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

        singleButton.setOnClickListener(
                v -> {
                    if (!data.id.equals(mSelectedItemId)) {
                        setSelection(position, data.id);
                    }
                });
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

    @Nullable
    String getSelectedItemIdForTesting() {
        return mSelectedItemId;
    }

    int getSelectedPositionForTesting() {
        return mSelectedPosition;
    }

    List<RichRadioButtonData> getOptionsForTesting() {
        return mOptions;
    }
}
