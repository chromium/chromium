// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.browser_ui.widget.chips.ChipViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds {@link ActionChipProperties} to a {@link ChipView}. */
@NullMarked
interface ActionChipViewBinder {
    static void bind(PropertyModel model, ChipView chip, PropertyKey key) {
        if (ActionChipProperties.RESOURCE_PROVIDER == key) {
            chip.setBackgroundTintList(
                    model.get(ActionChipProperties.RESOURCE_PROVIDER)
                            .getActionChipBackgroundTintList());
        } else {
            ChipViewBinder.bind(model, chip, key);
        }
    }
}
