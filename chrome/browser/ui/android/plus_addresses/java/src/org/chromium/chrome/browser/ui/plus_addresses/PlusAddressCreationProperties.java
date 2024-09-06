// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Defines the data model for the {@code PlusAddressCreationView}. */
class PlusAddressCreationProperties {
    static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("visible");

    static final PropertyKey[] ALL_KEYS = {
        VISIBLE,
    };

    static PropertyModel createDefaultModel() {
        return new PropertyModel.Builder(ALL_KEYS).with(VISIBLE, false).build();
    }

    private PlusAddressCreationProperties() {}
}
