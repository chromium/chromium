// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.whats_new;

import static org.chromium.chrome.browser.ui.android.whats_new.WhatsNewListItemProperties.DESCRIPTION_ID;
import static org.chromium.chrome.browser.ui.android.whats_new.WhatsNewListItemProperties.ICON_IMAGE_RES_ID;
import static org.chromium.chrome.browser.ui.android.whats_new.WhatsNewListItemProperties.ON_CLICK;
import static org.chromium.chrome.browser.ui.android.whats_new.WhatsNewListItemProperties.TITLE_ID;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for an individual line item in the What's New page's feature list. */
@NullMarked
public class WhatsNewListItemViewBinder {
    static void bind(PropertyModel model, WhatsNewListItemView view, PropertyKey propertyKey) {
        if (propertyKey.equals(TITLE_ID)) {
            view.setTitle(model.get(TITLE_ID));
        } else if (propertyKey.equals(DESCRIPTION_ID)) {
            view.setDescription(model.get(DESCRIPTION_ID));
        } else if (propertyKey.equals(ICON_IMAGE_RES_ID)) {
            view.setImage(model.get(ICON_IMAGE_RES_ID));
        } else if (propertyKey.equals(ON_CLICK)) {
            view.setOnClickListener(model.get(ON_CLICK));
        }
    }
}
