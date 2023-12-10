// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;
import androidx.core.util.ObjectsCompat;

import java.util.HashMap;
import java.util.Map;

/** An adapter for keeping track of which items to show in the dialog. */
public class DeviceItemAdapter extends ArrayAdapter<DeviceItemRow>
        implements AdapterView.OnItemClickListener {
    /** Item holder for performance boost. */
    private static class ViewHolder {
        private TextView mTextView;
        private @Nullable ImageView mImageView;

        public ViewHolder(View view) {
            mImageView = (ImageView) view.findViewById(R.id.icon);
            mTextView = (TextView) view.findViewById(R.id.description);
        }
    }

    /** An observer interface for item selection change in the adapter. */
    public interface Observer {
        /**
         * Called when item selection changed in the adapter.
         *
         * @param itemSelected Whether there is item selected in the adapter.
         */
        default void onItemSelectionChanged(boolean itemSelected) {}
    }

    private final LayoutInflater mInflater;

    private final Resources mResources;

    // True when there is need to select an item; false otherwise.
    private final boolean mItemsSelectable;

    private final @LayoutRes int mRowLayoutResource;

    // The zero-based index of the item currently selected in the dialog,
    // or -1 (INVALID_POSITION) if nothing is selected.
    private int mSelectedItem = ListView.INVALID_POSITION;

    // Item descriptions are counted in a map.
    private Map<String, Integer> mItemDescriptionMap = new HashMap<>();

    // Map of keys to items so that we can access the items in O(1).
    private Map<String, DeviceItemRow> mKeyToItemMap = new HashMap<>();

    // True when there is at least one row with an icon.
    private boolean mHasIcon;

    private @Nullable Observer mObserver;

    /**
     * Creates a device item adapter which can show a list of items.
     *
     * @param context The context used for layout inflation and resource loading.
     * @param rowLayoutResource The resource identifier for the item row.
     */
    public DeviceItemAdapter(
            Context context, boolean itemsSelectable, @LayoutRes int rowLayoutResource) {
        super(context, rowLayoutResource);

        mInflater = LayoutInflater.from(context);
        mResources = context.getResources();
        mItemsSelectable = itemsSelectable;
        mRowLayoutResource = rowLayoutResource;
    }

    @Override
    public boolean isEmpty() {
        boolean isEmpty = super.isEmpty();
        if (isEmpty) {
            assert mKeyToItemMap.isEmpty();
            assert mItemDescriptionMap.isEmpty();
        } else {
            assert !mKeyToItemMap.isEmpty();
            assert !mItemDescriptionMap.isEmpty();
        }
        return isEmpty;
    }

    /**
     * Adds an item to the list to show in the dialog if the item
     * was not in the chooser. Otherwise updates the items description, icon
     * and icon description.
     * @param key Unique identifier for that item.
     * @param description Text in the row.
     * @param icon Drawable to show next to the item.
     * @param iconDescription Description of the icon.
     */
    public void addOrUpdate(
            String key,
            String description,
            @Nullable Drawable icon,
            @Nullable String iconDescription) {
        DeviceItemRow oldItem = mKeyToItemMap.get(key);
        if (oldItem != null) {
            if (oldItem.hasSameContents(key, description, icon, iconDescription)) {
                // No need to update anything.
                return;
            }

            if (!TextUtils.equals(oldItem.mDescription, description)) {
                removeFromDescriptionsMap(oldItem.mDescription);
                oldItem.mDescription = description;
                addToDescriptionsMap(oldItem.mDescription);
            }

            if (!ObjectsCompat.equals(icon, oldItem.mIcon)) {
                oldItem.mIcon = icon;
                oldItem.mIconDescription = iconDescription;
            }

            notifyDataSetChanged();
            return;
        }

        assert !mKeyToItemMap.containsKey(key);
        DeviceItemRow newItem = new DeviceItemRow(key, description, icon, iconDescription);
        mKeyToItemMap.put(key, newItem);

        addToDescriptionsMap(newItem.mDescription);
        add(newItem);
    }

    public void removeItemWithKey(String key) {
        DeviceItemRow oldItem = mKeyToItemMap.remove(key);
        if (oldItem == null) return;
        int oldItemPosition = getPosition(oldItem);
        // If the removed item is the item that is currently selected, deselect it
        // and disable the confirm button. Otherwise if the removed item is before
        // the currently selected item, the currently selected item's index needs
        // to be adjusted by one.
        if (oldItemPosition == mSelectedItem) {
            updateSelectedItemPosition(ListView.INVALID_POSITION);
        } else if (oldItemPosition < mSelectedItem) {
            --mSelectedItem;
        }
        removeFromDescriptionsMap(oldItem.mDescription);
        super.remove(oldItem);
    }

    @Override
    public void clear() {
        mKeyToItemMap.clear();
        mItemDescriptionMap.clear();
        updateSelectedItemPosition(ListView.INVALID_POSITION);
        super.clear();
    }

    /**
     * Returns the key of the currently selected item or blank if nothing is
     * selected.
     */
    public String getSelectedItemKey() {
        if (mSelectedItem == ListView.INVALID_POSITION) return "";
        DeviceItemRow row = getItem(mSelectedItem);
        if (row == null) return "";
        return row.mKey;
    }

    /**
     * Returns the text to be displayed on the chooser for an item. For items with the same
     * description, their unique keys are appended to distinguish them.
     * @param position The index of the item.
     */
    public String getDisplayText(int position) {
        DeviceItemRow item = getItem(position);
        String description = item.mDescription;
        int counter = mItemDescriptionMap.get(description);
        return counter == 1
                ? description
                : mResources.getString(
                        R.string.item_chooser_item_name_with_id, description, item.mKey);
    }

    /**
     * Sets the observer to be notified of the item selection change in the adapter.
     * @param observer The observer to be notified.
     */
    public void setObserver(Observer observer) {
        mObserver = observer;
    }

    @Override
    public boolean isEnabled(int position) {
        return mItemsSelectable;
    }

    @Override
    public int getViewTypeCount() {
        return 1;
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        ViewHolder row;
        if (convertView == null) {
            convertView = mInflater.inflate(mRowLayoutResource, parent, false);
            row = new ViewHolder(convertView);
            convertView.setTag(row);
        } else {
            row = (ViewHolder) convertView.getTag();
        }

        row.mTextView.setSelected(position == mSelectedItem);
        row.mTextView.setEnabled(isEnabled(position));
        row.mTextView.setText(getDisplayText(position));

        if (row.mImageView != null) {
            // If there is at least one item with an icon then we set mImageView's
            // visibility to INVISIBLE for all items with no icons. We do this
            // so that all items' descriptions are aligned.
            if (!mHasIcon) {
                row.mImageView.setVisibility(View.GONE);
            } else {
                DeviceItemRow item = getItem(position);
                if (item.mIcon != null) {
                    row.mImageView.setContentDescription(item.mIconDescription);
                    row.mImageView.setImageDrawable(item.mIcon);
                    row.mImageView.setVisibility(View.VISIBLE);
                } else {
                    row.mImageView.setVisibility(View.INVISIBLE);
                    row.mImageView.setImageDrawable(null);
                    row.mImageView.setContentDescription(null);
                }
                row.mImageView.setSelected(position == mSelectedItem);
            }
        }

        return convertView;
    }

    @Override
    public void notifyDataSetChanged() {
        mHasIcon = false;
        for (DeviceItemRow row : mKeyToItemMap.values()) {
            if (row.mIcon != null) mHasIcon = true;
        }
        super.notifyDataSetChanged();
    }

    @Override
    public void onItemClick(AdapterView<?> adapter, View view, int position, long id) {
        assert mItemsSelectable;
        updateSelectedItemPosition(position);
        notifyDataSetChanged();
    }

    private void addToDescriptionsMap(String description) {
        int count =
                mItemDescriptionMap.containsKey(description)
                        ? mItemDescriptionMap.get(description)
                        : 0;
        mItemDescriptionMap.put(description, count + 1);
    }

    private void removeFromDescriptionsMap(String description) {
        if (!mItemDescriptionMap.containsKey(description)) {
            return;
        }
        int count = mItemDescriptionMap.get(description);
        if (count == 1) {
            mItemDescriptionMap.remove(description);
        } else {
            mItemDescriptionMap.put(description, count - 1);
        }
    }

    private void updateSelectedItemPosition(int position) {
        mSelectedItem = position;
        if (mObserver != null) {
            boolean itemSelected = (mSelectedItem != ListView.INVALID_POSITION);
            mObserver.onItemSelectionChanged(itemSelected);
        }
    }
}
