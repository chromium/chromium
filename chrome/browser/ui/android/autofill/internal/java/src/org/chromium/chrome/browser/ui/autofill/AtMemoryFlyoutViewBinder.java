// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds properties for the AtMemoryFlyout. */
@NullMarked
class AtMemoryFlyoutViewBinder {
    private AtMemoryFlyoutViewBinder() {}

    static void bind(PropertyModel model, AtMemoryFlyoutView view, PropertyKey propertyKey) {
        if (propertyKey == AtMemoryFlyoutProperties.TITLE) {
            view.setTitle(model.get(AtMemoryFlyoutProperties.TITLE));
        } else if (propertyKey == AtMemoryFlyoutProperties.SOURCE_TEXT) {
            view.setSourceText(model.get(AtMemoryFlyoutProperties.SOURCE_TEXT));
        } else if (propertyKey == AtMemoryFlyoutProperties.CHIPS_DATA) {
            view.setChipsData(model.get(AtMemoryFlyoutProperties.CHIPS_DATA));
        } else if (propertyKey == AtMemoryFlyoutProperties.ON_BACK_CLICKED) {
            view.setBackButtonCallback(model.get(AtMemoryFlyoutProperties.ON_BACK_CLICKED));
        } else if (propertyKey == AtMemoryFlyoutProperties.ON_SOURCE_CLICKED) {
            view.setSourceClickCallback(model.get(AtMemoryFlyoutProperties.ON_SOURCE_CLICKED));
        } else if (propertyKey == AtMemoryFlyoutProperties.ON_MANAGE_CLICKED) {
            view.setManageClickCallback(model.get(AtMemoryFlyoutProperties.ON_MANAGE_CLICKED));
        } else if (propertyKey == AtMemoryFlyoutProperties.ON_CHIP_CLICKED) {
            view.setChipClickCallback(model.get(AtMemoryFlyoutProperties.ON_CHIP_CLICKED));
        } else {
            assert false : "Unhandled property: " + propertyKey;
        }
    }
}
