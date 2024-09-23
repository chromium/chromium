// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.chips;

import android.text.TextUtils;

import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder to bind a model to a {@link ChipView}. */
public class ChipViewBinder {
    public static void bind(PropertyModel model, ChipView chip, PropertyKey key) {
        if (ChipProperties.CLICK_HANDLER == key) {
            chip.setOnClickListener((v) -> model.get(ChipProperties.CLICK_HANDLER).onResult(model));

        } else if (ChipProperties.CONTENT_DESCRIPTION == key) {
            chip.getPrimaryTextView()
                    .setContentDescription(model.get(ChipProperties.CONTENT_DESCRIPTION));

        } else if (ChipProperties.ENABLED == key) {
            chip.setEnabled(model.get(ChipProperties.ENABLED));

        } else if (ChipProperties.ICON == key || ChipProperties.APPLY_ICON_TINT == key) {
            int iconId = model.get(ChipProperties.ICON);
            if (iconId != ChipProperties.INVALID_ICON_ID) {
                // TODO: Revisit the logic below:
                // - avoid overriding supplied icon, make no assumptions about how this is used.
                // - override won't work if SELECTED property is applied after ICON.
                boolean isSelected =
                        model.getAllSetProperties().contains(ChipProperties.SELECTED)
                                && model.get(ChipProperties.SELECTED);
                chip.setIcon(
                        isSelected ? R.drawable.ic_check_googblue_24dp : iconId,
                        model.get(ChipProperties.APPLY_ICON_TINT));
            } else {
                chip.setIcon(ChipProperties.INVALID_ICON_ID, false);
            }

        } else if (ChipProperties.ID == key) {
            // Intentional noop.

        } else if (ChipProperties.PRIMARY_TEXT_APPEARANCE == key) {
            chip.getPrimaryTextView()
                    .setTextAppearance(model.get(ChipProperties.PRIMARY_TEXT_APPEARANCE));
        } else if (ChipProperties.SELECTED == key) {
            chip.setSelected(model.get(ChipProperties.SELECTED));
        } else if (ChipProperties.TEXT == key) {
            chip.getPrimaryTextView().setText(model.get(ChipProperties.TEXT));
        } else if (ChipProperties.TEXT_MAX_WIDTH_PX == key) {
            int widthPx = model.get(ChipProperties.TEXT_MAX_WIDTH_PX);
            if (widthPx == ChipProperties.SHOW_WHOLE_TEXT) {
                chip.getPrimaryTextView().setEllipsize(null);
                chip.getPrimaryTextView().setMaxWidth(Integer.MAX_VALUE);
            } else {
                chip.getPrimaryTextView().setEllipsize(TextUtils.TruncateAt.END);
                chip.getPrimaryTextView().setMaxWidth(widthPx);
            }
        }
    }
}
