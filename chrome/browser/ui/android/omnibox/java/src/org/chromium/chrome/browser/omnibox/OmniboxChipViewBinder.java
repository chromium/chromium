// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.AVAILABLE_WIDTH;
import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.CONTENT_DESC;
import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.ICON;
import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.ON_CLICK;
import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.TEXT;

import android.view.View;

import androidx.appcompat.widget.TooltipCompat;

import com.google.android.material.button.MaterialButton;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the omnibox chip. */
@NullMarked
class OmniboxChipViewBinder {
    public static void bind(PropertyModel model, MaterialButton view, PropertyKey propertyKey) {
        if (TEXT.equals(propertyKey)) {
            view.setText(model.get(TEXT));
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
        }
    }
}
