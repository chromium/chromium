// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Defines the data model for the {@code PlusAddressCreationView}. */
class PlusAddressCreationProperties {
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey("visible");
    static final WritableObjectPropertyKey<String> PROPOSED_PLUS_ADDRESS =
            new WritableObjectPropertyKey<>("proposed_plus_address");
    static final ReadableObjectPropertyKey<PlusAddressCreationDelegate> DELEGATE =
            new ReadableObjectPropertyKey<>("delegate");

    static final PropertyKey[] ALL_KEYS = {
        VISIBLE, PROPOSED_PLUS_ADDRESS, DELEGATE,
    };

    private PlusAddressCreationProperties() {}
}
