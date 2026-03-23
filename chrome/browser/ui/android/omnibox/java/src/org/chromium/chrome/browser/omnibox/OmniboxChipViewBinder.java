// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.AVAILABLE_WIDTH;
import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.CONTENT_DESC;
import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.ICON;
import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.ON_CLICK;
import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.TEXT;

import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;

import androidx.annotation.Px;
import androidx.appcompat.widget.TooltipCompat;

import com.google.android.material.button.MaterialButton;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.AttrUtils;

/** View binder for the omnibox chip. */
@NullMarked
class OmniboxChipViewBinder {
    public static void bind(PropertyModel model, MaterialButton view, PropertyKey propertyKey) {
        if (TEXT.equals(propertyKey)) {
            updatePaddingAndText(view, model);
        } else if (ICON.equals(propertyKey)) {
            view.setIcon(model.get(ICON));
        } else if (CONTENT_DESC.equals(propertyKey)) {
            String contentDesc = model.get(CONTENT_DESC);
            view.setContentDescription(contentDesc);
            TooltipCompat.setTooltipText(view, contentDesc);
        } else if (ON_CLICK.equals(propertyKey)) {
            Runnable onClick = model.get(ON_CLICK);
            view.setOnClickListener(
                    v -> {
                        if (onClick != null) onClick.run();
                    });
        } else if (AVAILABLE_WIDTH.equals(propertyKey)) {
            int availableWidth = model.get(AVAILABLE_WIDTH);
            view.setVisibility(availableWidth > 0 ? View.VISIBLE : View.GONE);
            view.setMaxWidth(availableWidth);
            updatePaddingAndText(view, model);
        }
    }

    private static void updatePaddingAndText(MaterialButton view, PropertyModel model) {
        var context = view.getContext();
        var res = context.getResources();
        @Px
        int collapsedWidth = AttrUtils.getDimensionPixelSize(context, R.attr.minInteractTargetSize);
        boolean isCollapsed =
                model.get(AVAILABLE_WIDTH) <= collapsedWidth || TextUtils.isEmpty(model.get(TEXT));

        @Px int paddingVertical = res.getDimensionPixelSize(R.dimen.omnibox_chip_padding);

        if (isCollapsed) {
            @Px
            int paddingHorizontal =
                    res.getDimensionPixelSize(R.dimen.omnibox_chip_padding_horizontal_collapsed);
            view.setText("");
            view.setIconPadding(0);
            view.setGravity(Gravity.CENTER);
            view.setPaddingRelative(
                    paddingHorizontal, paddingVertical, paddingHorizontal, paddingVertical);
        } else {
            @Px
            int paddingStart =
                    res.getDimensionPixelSize(
                            R.dimen.omnibox_chip_padding_horizontal_expanded_start);
            @Px
            int paddingEnd =
                    res.getDimensionPixelSize(R.dimen.omnibox_chip_padding_horizontal_expanded_end);
            @Px int iconPadding = res.getDimensionPixelSize(R.dimen.omnibox_chip_padding);
            view.setText(model.get(TEXT));
            view.setIconPadding(iconPadding);
            view.setGravity(Gravity.CENTER_VERTICAL | Gravity.START);
            view.setPaddingRelative(paddingStart, paddingVertical, paddingEnd, paddingVertical);
        }
    }
}
