// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Properties defined here reflect the state of the AccountSelection-components.
 */
class AccountSelectionProperties {
    static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("visible");

    static final PropertyModel.ReadableObjectPropertyKey<Callback<Integer>> DISMISS_HANDLER =
            new PropertyModel.ReadableObjectPropertyKey<>("dismiss_handler");

    static PropertyModel createDefaultModel(Callback<Integer> handler) {
        return new PropertyModel.Builder(VISIBLE, DISMISS_HANDLER)
                .with(VISIBLE, false)
                .with(DISMISS_HANDLER, handler)
                .build();
    }

    private AccountSelectionProperties() {}
}
