// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the page zoom feature. */
class PageZoomProperties {
    static final WritableObjectPropertyKey<Callback<Void>> DECREASE_ZOOM_CALLBACK =
            new WritableObjectPropertyKey<Callback<Void>>();
    static final WritableObjectPropertyKey<Callback<Void>> INCREASE_ZOOM_CALLBACK =
            new WritableObjectPropertyKey<Callback<Void>>();
    static final WritableObjectPropertyKey<Callback<Void>> RESET_ZOOM_CALLBACK =
            new WritableObjectPropertyKey<Callback<Void>>();
    static final WritableObjectPropertyKey<Callback<Integer>> SEEKBAR_CHANGE_CALLBACK =
            new WritableObjectPropertyKey<Callback<Integer>>();
    static final WritableObjectPropertyKey<Callback<Void>> USER_INTERACTION_CALLBACK =
            new WritableObjectPropertyKey<Callback<Void>>();

    static final WritableBooleanPropertyKey DECREASE_ZOOM_ENABLED =
            new WritableBooleanPropertyKey();
    static final WritableBooleanPropertyKey INCREASE_ZOOM_ENABLED =
            new WritableBooleanPropertyKey();
    static final WritableBooleanPropertyKey RESET_ZOOM_VISIBLE = new WritableBooleanPropertyKey();

    static final WritableIntPropertyKey MAXIMUM_SEEK_VALUE = new WritableIntPropertyKey();
    static final WritableIntPropertyKey CURRENT_SEEK_VALUE = new WritableIntPropertyKey();

    static final WritableObjectPropertyKey<Double> DEFAULT_ZOOM_FACTOR =
            new WritableObjectPropertyKey<Double>();

    static final PropertyKey[] ALL_KEYS = {
        DECREASE_ZOOM_CALLBACK,
        INCREASE_ZOOM_CALLBACK,
        RESET_ZOOM_CALLBACK,
        SEEKBAR_CHANGE_CALLBACK,
        USER_INTERACTION_CALLBACK,
        DECREASE_ZOOM_ENABLED,
        INCREASE_ZOOM_ENABLED,
        RESET_ZOOM_VISIBLE,
        MAXIMUM_SEEK_VALUE,
        CURRENT_SEEK_VALUE,
        DEFAULT_ZOOM_FACTOR
    };
}
