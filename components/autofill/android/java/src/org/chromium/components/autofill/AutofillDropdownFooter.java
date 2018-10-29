// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.Context;
import android.support.v7.content.res.AppCompatResources;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.ui.DropdownItem;

import java.util.List;

/**
 * Renders the footer items in the Autofill dropdown, as provided by a List of DropdownItem objects.
 */
public class AutofillDropdownFooter extends LinearLayout {
    /**
     * Interface for handling selection events within the footer.
     */
    public interface Observer {
        /**
         * Invoked when an item in the footer is selected.
         * @param item The data represented by this row.
         */
        void onFooterSelection(DropdownItem item);
    }

    /**
     * View representing a single row in the footer.
     */
    private class FooterRow extends FrameLayout implements OnClickListener {
        private DropdownItem mItem;
        private Observer mObserver;

        /**
         * @param context Application context.
         * @param item The DropdownItem representing the Autofill suggestion for this footer option.
         * @param observer An object capable of responding to a selection of this row.
         */
        public FooterRow(Context context, DropdownItem item, Observer observer) {
            super(context);
            mItem = item;
            mObserver = observer;
            inflate(context, R.layout.autofill_dropdown_footer_item_refresh, this);
            TextView label = findViewById(R.id.dropdown_label);
            label.setText(item.getLabel());

            ImageView icon = findViewById(R.id.dropdown_icon);
            if (item.getIconId() == DropdownItem.NO_ICON) {
                icon.setVisibility(View.GONE);
            } else {
                icon.setImageDrawable(AppCompatResources.getDrawable(context, item.getIconId()));
            }

            setOnClickListener(this);
        }

        @Override
        public void onClick(View v) {
            mObserver.onFooterSelection(mItem);
        }
    }

    private Context mContext;

    /**
     * @param context Application context.
     * @param items The item or items representing the content to be displayed in the footer. Each
     *              item will be rendered as a row.
     * @param observer An object capable of responding to a selection of a row in the footer.
     */
    public AutofillDropdownFooter(Context context, List<DropdownItem> items, Observer observer) {
        super(context);
        mContext = context;

        setOrientation(LinearLayout.VERTICAL);

        for (DropdownItem item : items) {
            addView(new FooterRow(context, item, observer));
        }
    }
}
