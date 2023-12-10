// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.detail_screen;

import androidx.annotation.StringRes;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/** The properties of a footer item on one of the FastCheckout detail screens. */
public class FooterItemProperties {
    /** The resource id of the string to be shown on the footer label. */
    public static final ReadableIntPropertyKey LABEL = new ReadableIntPropertyKey("label");

    /** The Runnable that is executed when the footer is clicked. */
    public static final ReadableObjectPropertyKey<Runnable> ON_CLICK_HANDLER =
            new ReadableObjectPropertyKey("on_click_handler");

    public static PropertyModel create(@StringRes int label, Runnable onClickHandler) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(LABEL, label)
                .with(ON_CLICK_HANDLER, onClickHandler)
                .build();
    }

    /** All keys used for the fast checkout footer item. */
    static final PropertyKey[] ALL_KEYS = new PropertyKey[] {LABEL, ON_CLICK_HANDLER};
}
