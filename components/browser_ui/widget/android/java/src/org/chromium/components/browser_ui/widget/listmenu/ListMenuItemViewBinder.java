// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.listmenu;

import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;

/**
 * Class responsible for binding the model of the ListMenuItem and the view.
 */
public class ListMenuItemViewBinder {
    public static void binder(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView textView = view.findViewById(R.id.menu_item_text);
        ImageView startIcon = view.findViewById(R.id.menu_item_icon);
        ImageView endIcon = view.findViewById(R.id.menu_item_end_icon);
        if (propertyKey == ListMenuItemProperties.TITLE_ID) {
            textView.setText(model.get(ListMenuItemProperties.TITLE_ID));
        } else if (propertyKey == ListMenuItemProperties.TITLE) {
            textView.setText(model.get(ListMenuItemProperties.TITLE));
        } else if (propertyKey == ListMenuItemProperties.CONTENT_DESCRIPTION) {
            textView.setContentDescription(model.get(ListMenuItemProperties.CONTENT_DESCRIPTION));
        } else if (propertyKey == ListMenuItemProperties.START_ICON_ID
                || propertyKey == ListMenuItemProperties.END_ICON_ID) {
            int id = model.get((ReadableIntPropertyKey) propertyKey);
            Drawable drawable =
                    id == 0 ? null : AppCompatResources.getDrawable(view.getContext(), id);
            if (drawable != null) {
                if (propertyKey == ListMenuItemProperties.START_ICON_ID) {
                    // need more space between the start and the icon if icon is on the start.
                    startIcon.setImageDrawable(drawable);
                    textView.setPaddingRelative(
                            view.getResources().getDimensionPixelOffset(R.dimen.menu_padding_start),
                            textView.getPaddingTop(), textView.getPaddingEnd(),
                            textView.getPaddingBottom());
                    startIcon.setVisibility(View.VISIBLE);
                    endIcon.setVisibility(View.GONE);
                } else {
                    // Move to the end.
                    endIcon.setImageDrawable(drawable);
                    startIcon.setVisibility(View.GONE);
                    endIcon.setVisibility(View.VISIBLE);
                }
            }
        } else if (propertyKey == ListMenuItemProperties.MENU_ITEM_ID) {
            // Not tracked intentionally because it's mainly for clients to know which menu item is
            // clicked.
        } else if (propertyKey == ListMenuItemProperties.ENABLED) {
            textView.setEnabled(model.get(ListMenuItemProperties.ENABLED));
            startIcon.setEnabled(model.get(ListMenuItemProperties.ENABLED));
            endIcon.setEnabled(model.get(ListMenuItemProperties.ENABLED));
        } else if (propertyKey == ListMenuItemProperties.TINT_COLOR_ID) {
            ImageViewCompat.setImageTintList(startIcon,
                    AppCompatResources.getColorStateList(
                            view.getContext(), model.get(ListMenuItemProperties.TINT_COLOR_ID)));
            ImageViewCompat.setImageTintList(endIcon,
                    AppCompatResources.getColorStateList(
                            view.getContext(), model.get(ListMenuItemProperties.TINT_COLOR_ID)));
        } else if (propertyKey == ListMenuItemProperties.TEXT_APPEARANCE_ID) {
            ApiCompatibilityUtils.setTextAppearance(
                    textView, model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
        } else if (propertyKey == ListMenuItemProperties.IS_TEXT_ELLIPSIZED_AT_END) {
            if (model.get(ListMenuItemProperties.IS_TEXT_ELLIPSIZED_AT_END)) {
                textView.setMaxLines(1);
                textView.setEllipsize(TextUtils.TruncateAt.END);
            } else {
                textView.setEllipsize(null);
            }
        } else {
            assert false : "Supplied propertyKey not implemented in ListMenuItemProperties.";
        }
    }
}
