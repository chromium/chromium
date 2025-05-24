// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.DropdownDividerDrawable;
import org.chromium.ui.DropdownItem;

import java.util.List;
import java.util.Set;

/** Dropdown item adapter for the AutofillPopup. */
@NullMarked
public class AutofillDropdownAdapter extends ArrayAdapter<DropdownItem> {
    private final Context mContext;
    private final Set<Integer> mSeparators;
    private final boolean mAreAllItemsEnabled;

    /**
     * Creates an {@code ArrayAdapter} with specified parameters.
     * @param context Application context.
     * @param items List of labels and icons to display.
     * @param separators Set of positions that separate {@code items}.
     */
    public AutofillDropdownAdapter(
            Context context, List<? extends DropdownItem> items, Set<Integer> separators) {
        super(context, R.layout.autofill_dropdown_item);
        mContext = context;
        addAll(items);
        mSeparators = separators;
        mAreAllItemsEnabled = checkAreAllItemsEnabled();
    }

    private boolean checkAreAllItemsEnabled() {
        for (int i = 0; i < getCount(); i++) {
            DropdownItem item = assumeNonNull(getItem(i));
            if (item.isEnabled() && !item.isGroupHeader()) {
                return false;
            }
        }
        return true;
    }

    @Override
    public View getView(int position, @Nullable View convertView, ViewGroup parent) {
        View layout = convertView;
        if (layout == null) {
            LayoutInflater inflater = LayoutInflater.from(mContext);
            layout = inflater.inflate(R.layout.autofill_dropdown_item, null);
            layout.setBackground(new DropdownDividerDrawable(/* backgroundColor= */ null));
        }

        DropdownItem item = getItem(position);
        assert item != null;

        int height =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.autofill_dropdown_item_height);

        DropdownDividerDrawable divider = (DropdownDividerDrawable) layout.getBackground();
        if (position == 0) {
            divider.setDividerColor(Color.TRANSPARENT);
        } else {
            int dividerHeight =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.autofill_dropdown_item_divider_height);
            height += dividerHeight;
            divider.setHeight(dividerHeight);
            int dividerColor;
            if (mSeparators != null && mSeparators.contains(position)) {
                dividerColor = mContext.getColor(R.color.dropdown_dark_divider_color);
            } else {
                dividerColor = mContext.getColor(R.color.dropdown_divider_color);
            }
            divider.setDividerColor(dividerColor);
        }

        // Note: trying to set the height of the root LinearLayout breaks accessibility,
        // so we have to adjust the height of this LinearLayout that wraps the TextViews
        // instead. If you need to modify this layout, don't forget to test it with TalkBack and
        // make sure it doesn't regress. http://crbug.com/429364
        LinearLayout wrapper = (LinearLayout) layout.findViewById(R.id.dropdown_label_wrapper);
        wrapper.setOrientation(LinearLayout.VERTICAL);
        wrapper.setLayoutParams(new LinearLayout.LayoutParams(0, height, 1));

        // Layout of the main label view.
        TextView labelView =
                assumeNonNull(
                        populateLabelView(
                                layout, R.id.dropdown_label, item.getLabel(), item.isEnabled()));
        TextView secondaryLabelView =
                populateLabelView(
                        layout,
                        R.id.dropdown_secondary_label,
                        item.getSecondaryLabel(),
                        item.isEnabled());
        labelView.setSingleLine(true);

        if (item.isGroupHeader()) {
            labelView.setTypeface(null, Typeface.BOLD);
            if (secondaryLabelView != null) {
                secondaryLabelView.setTypeface(null, Typeface.BOLD);
            }
        } else {
            labelView.setTypeface(null, Typeface.NORMAL);
            if (secondaryLabelView != null) {
                secondaryLabelView.setTypeface(null, Typeface.NORMAL);
            }
        }

        labelView.setTextSize(
                TypedValue.COMPLEX_UNIT_PX,
                mContext.getResources().getDimension(R.dimen.text_size_large));
        labelView.setTextColor(mContext.getColor(item.getLabelFontColorResId()));

        if (secondaryLabelView != null) {
            secondaryLabelView.setTextSize(
                    TypedValue.COMPLEX_UNIT_PX,
                    mContext.getResources().getDimension(R.dimen.text_size_large));
            secondaryLabelView.setTextColor(mContext.getColor(item.getLabelFontColorResId()));
        }

        // Layout of the sublabel view, which has a smaller font and usually sits below the main
        // label.
        TextView sublabelView =
                populateLabelView(layout, R.id.dropdown_sublabel, item.getSublabel(), false);
        if (sublabelView != null) {
            sublabelView.setTextSize(
                    TypedValue.COMPLEX_UNIT_PX,
                    mContext.getResources().getDimension(R.dimen.text_size_small));
            sublabelView.setTextColor(
                    mContext.getColor(R.color.default_text_color_secondary_list_baseline));
        }

        TextView secondarySublabelView =
                populateLabelView(
                        layout,
                        R.id.dropdown_secondary_sublabel,
                        item.getSecondarySublabel(),
                        false);
        if (secondarySublabelView != null) {
            secondarySublabelView.setTextSize(
                    TypedValue.COMPLEX_UNIT_PX,
                    mContext.getResources().getDimension(R.dimen.text_size_small));
            secondarySublabelView.setTextColor(
                    mContext.getColor(R.color.default_text_color_secondary_list_baseline));
        }

        return layout;
    }

    @Override
    public boolean areAllItemsEnabled() {
        return mAreAllItemsEnabled;
    }

    @Override
    public boolean isEnabled(int position) {
        if (position < 0 || position >= getCount()) return false;
        DropdownItem item = assumeNonNull(getItem(position));
        return item.isEnabled() && !item.isGroupHeader();
    }

    /**
     * Sets the text and the enabled state for the dropdown labels.
     *
     * @param item the DropdownItem for this row.
     * @param layout the View in which the label can be found.
     * @param viewId the ID for the label's view.
     * @param label the text to be displayed as the label.
     * @param isEnabled the android:enabled state of the label.
     * @return the View.
     */
    private @Nullable TextView populateLabelView(
            View layout, int viewId, @Nullable CharSequence label, boolean isEnabled) {
        TextView labelView = layout.findViewById(viewId);
        if (TextUtils.isEmpty(label)) {
            labelView.setVisibility(View.GONE);
            return null;
        }
        labelView.setText(label);
        labelView.setEnabled(isEnabled);
        labelView.setVisibility(View.VISIBLE);
        return labelView;
    }
}
