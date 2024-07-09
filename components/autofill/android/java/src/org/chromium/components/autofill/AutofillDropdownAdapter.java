// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AbsListView.LayoutParams;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.view.MarginLayoutParamsCompat;
import androidx.core.view.ViewCompat;

import org.chromium.ui.DropdownDividerDrawable;
import org.chromium.ui.DropdownItem;

import java.util.List;
import java.util.Set;

/** Dropdown item adapter for the AutofillPopup. */
public class AutofillDropdownAdapter extends ArrayAdapter<DropdownItem> {
    private final Context mContext;
    private final Set<Integer> mSeparators;
    private final boolean mAreAllItemsEnabled;
    private final int mLabelMargin;

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
        mLabelMargin =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.autofill_dropdown_item_label_margin);
    }

    private boolean checkAreAllItemsEnabled() {
        for (int i = 0; i < getCount(); i++) {
            DropdownItem item = getItem(i);
            if (item.isEnabled() && !item.isGroupHeader()) {
                return false;
            }
        }
        return true;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View layout = convertView;
        if (convertView == null) {
            LayoutInflater inflater = LayoutInflater.from(mContext);
            layout = inflater.inflate(R.layout.autofill_dropdown_item, null);
            layout.setBackground(new DropdownDividerDrawable(/* backgroundColor= */ null));
        }

        DropdownItem item = getItem(position);

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

        // Layout of the item tag view, which has a smaller font and sits below the sub
        // label.
        TextView itemTagView =
                populateLabelView(layout, R.id.dropdown_item_tag, item.getItemTag(), false);
        if (itemTagView != null) {
            itemTagView.setTextSize(
                    TypedValue.COMPLEX_UNIT_PX,
                    mContext.getResources().getDimension(item.getSublabelFontSizeResId()));
            itemTagView.setTextColor(mContext.getColor(item.getSublabelFontColorResId()));
            height +=
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.autofill_dropdown_item_tag_height);
        }

        // Note: trying to set the height of the root LinearLayout breaks accessibility,
        // so we have to adjust the height of this LinearLayout that wraps the TextViews
        // instead. If you need to modify this layout, don't forget to test it with TalkBack and
        // make sure it doesn't regress. http://crbug.com/429364
        LinearLayout wrapper = (LinearLayout) layout.findViewById(R.id.dropdown_label_wrapper);
        if (item.isMultilineLabel()) height = LayoutParams.WRAP_CONTENT;
        wrapper.setOrientation(LinearLayout.VERTICAL);
        wrapper.setLayoutParams(new LinearLayout.LayoutParams(0, height, 1));

        // Layout of the main label view.
        TextView labelView =
                populateLabelView(layout, R.id.dropdown_label, item.getLabel(), item.isEnabled());
        TextView secondaryLabelView =
                populateLabelView(
                        layout,
                        R.id.dropdown_secondary_label,
                        item.getSecondaryLabel(),
                        item.isEnabled());
        labelView.setSingleLine(!item.isMultilineLabel());
        if (item.isMultilineLabel()) {
            // If there is a multiline label, we add extra padding at the top and bottom because
            // WRAP_CONTENT, defined above for multiline labels, leaves none.
            int existingStart = ViewCompat.getPaddingStart(labelView);
            int existingEnd = ViewCompat.getPaddingEnd(labelView);
            labelView.setPaddingRelative(existingStart, mLabelMargin, existingEnd, mLabelMargin);
        }

        if (item.isGroupHeader() || item.isBoldLabel()) {
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
                mContext.getResources().getDimension(item.getLabelFontSizeResId()));
        labelView.setTextColor(mContext.getColor(item.getLabelFontColorResId()));

        if (secondaryLabelView != null) {
            secondaryLabelView.setTextSize(
                    TypedValue.COMPLEX_UNIT_PX,
                    mContext.getResources().getDimension(item.getLabelFontSizeResId()));
            secondaryLabelView.setTextColor(mContext.getColor(item.getLabelFontColorResId()));
        }

        // Layout of the sublabel view, which has a smaller font and usually sits below the main
        // label.
        TextView sublabelView =
                populateLabelView(layout, R.id.dropdown_sublabel, item.getSublabel(), false);
        if (sublabelView != null) {
            sublabelView.setTextSize(
                    TypedValue.COMPLEX_UNIT_PX,
                    mContext.getResources().getDimension(item.getSublabelFontSizeResId()));
            sublabelView.setTextColor(mContext.getColor(item.getSublabelFontColorResId()));
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
                    mContext.getResources().getDimension(item.getSublabelFontSizeResId()));
            secondarySublabelView.setTextColor(mContext.getColor(item.getSublabelFontColorResId()));
        }

        ImageView iconViewStart = (ImageView) layout.findViewById(R.id.start_dropdown_icon);
        ImageView iconViewEnd = (ImageView) layout.findViewById(R.id.end_dropdown_icon);
        if (item.isIconAtStart()) {
            iconViewEnd.setVisibility(View.GONE);
            iconViewStart.setVisibility(View.VISIBLE);
        } else {
            iconViewStart.setVisibility(View.GONE);
            iconViewEnd.setVisibility(View.VISIBLE);
        }

        ImageView iconView =
                populateIconView(item.isIconAtStart() ? iconViewStart : iconViewEnd, item);
        if (iconView != null) {
            iconView.setLayoutParams(getSizeAndMarginParamsForIconView(iconView, item));
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
        DropdownItem item = getItem(position);
        return item.isEnabled() && !item.isGroupHeader();
    }

    /**
     * Sets the text and the enabled state for the dropdown labels.
     * @param item the DropdownItem for this row.
     * @param layout the View in which the label can be found.
     * @param viewId the ID for the label's view.
     * @param label the text to be displayed as the label.
     * @param isEnabled the android:enabled state of the label.
     * @return the View.
     */
    private TextView populateLabelView(
            View layout, int viewId, CharSequence label, boolean isEnabled) {
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

    /**
     * Sets the drawable in the given ImageView to the resource identified in the item, or sets
     * iconView to visibility GONE if no icon is given.
     * @param iconView the ImageView which should be modified.
     * @param item the DropdownItem for this row.
     * @return |iconView| if it has been set to be visible; null otherwise.
     */
    @Nullable
    private ImageView populateIconView(ImageView iconView, DropdownItem item) {
        // If there is no icon, remove the icon view.
        if (item.getIconDrawable() == null) {
            iconView.setVisibility(View.GONE);
            return null;
        }
        iconView.setImageDrawable(item.getIconDrawable());
        iconView.setVisibility(View.VISIBLE);
        // TODO(crbug.com/40589327): Add accessible text for this icon.
        return iconView;
    }

    /**
     * @param iconView the ImageView for which params are being generated.
     * @param item the DropdownItem for this row.
     * @return a MarginLayoutParams object with values suitable for sizing iconView.
     */
    private ViewGroup.MarginLayoutParams getSizeParamsForIconView(
            ImageView iconView, DropdownItem item) {
        ViewGroup.MarginLayoutParams iconLayoutParams =
                (ViewGroup.MarginLayoutParams) iconView.getLayoutParams();
        int iconSizeResId = item.getIconSizeResId();
        int iconSize =
                iconSizeResId == 0
                        ? LayoutParams.WRAP_CONTENT
                        : mContext.getResources().getDimensionPixelSize(iconSizeResId);
        iconLayoutParams.width = iconSize;
        iconLayoutParams.height = iconSize;
        return iconLayoutParams;
    }

    /**
     * @param iconView the ImageView for which params are being generated.
     * @param item the DropdownItem for this row.
     * @return the same as |getSizeParamsForIconView|, but with additional margin-related params
     * set.
     */
    private ViewGroup.MarginLayoutParams getSizeAndMarginParamsForIconView(
            ImageView iconView, DropdownItem item) {
        ViewGroup.MarginLayoutParams params = getSizeParamsForIconView(iconView, item);
        int iconMargin = mContext.getResources().getDimensionPixelSize(item.getIconMarginResId());
        MarginLayoutParamsCompat.setMarginStart(params, iconMargin);
        MarginLayoutParamsCompat.setMarginEnd(params, iconMargin);
        return params;
    }
}
