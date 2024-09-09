// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Defines the data model for the {@code PlusAddressCreationView}. */
class PlusAddressCreationProperties {
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey("visible");
    static final WritableObjectPropertyKey<String> PROPOSED_PLUS_ADDRESS =
            new WritableObjectPropertyKey<>("proposed_plus_address");

    static final PropertyKey[] ALL_KEYS = {
        VISIBLE, PROPOSED_PLUS_ADDRESS,
    };

    static PropertyModel createDefaultModel() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(VISIBLE, false)
                .with(PROPOSED_PLUS_ADDRESS, "")
                .build();
    }

    private PlusAddressCreationProperties() {}
}
