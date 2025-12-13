// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the page zoom feature. */
@NullMarked
public class PageZoomProperties {
    public static final WritableObjectPropertyKey<Runnable> DECREASE_ZOOM_CALLBACK =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> INCREASE_ZOOM_CALLBACK =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> RESET_ZOOM_CALLBACK =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey DECREASE_ZOOM_ENABLED =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey INCREASE_ZOOM_ENABLED =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<Double> DEFAULT_ZOOM_FACTOR =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> ZOOM_PERCENT_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> IMMERIVE_MODE_CALLBACK =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Callback<Integer>> BAR_VALUE_CHANGE_CALLBACK =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Callback<@Nullable Void>> USER_INTERACTION_CALLBACK =
            new WritableObjectPropertyKey<>();
    static final WritableIntPropertyKey MAXIMUM_BAR_VALUE = new WritableIntPropertyKey();
    static final WritableIntPropertyKey CURRENT_BAR_VALUE = new WritableIntPropertyKey();
    static final ReadableBooleanPropertyKey USE_SLIDER = new ReadableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        DECREASE_ZOOM_CALLBACK,
        INCREASE_ZOOM_CALLBACK,
        RESET_ZOOM_CALLBACK,
        BAR_VALUE_CHANGE_CALLBACK,
        USER_INTERACTION_CALLBACK,
        DECREASE_ZOOM_ENABLED,
        INCREASE_ZOOM_ENABLED,
        MAXIMUM_BAR_VALUE,
        CURRENT_BAR_VALUE,
        USE_SLIDER,
        DEFAULT_ZOOM_FACTOR,
        ZOOM_PERCENT_TEXT
    };

    public static final PropertyKey[] ALL_KEYS_FOR_MENU_ITEM = {
        DECREASE_ZOOM_CALLBACK,
        INCREASE_ZOOM_CALLBACK,
        DECREASE_ZOOM_ENABLED,
        INCREASE_ZOOM_ENABLED,
        DEFAULT_ZOOM_FACTOR,
        ZOOM_PERCENT_TEXT,
        IMMERIVE_MODE_CALLBACK
    };

    public static final PropertyKey[] ALL_KEYS_FOR_INDICATOR = {
        DECREASE_ZOOM_CALLBACK,
        INCREASE_ZOOM_CALLBACK,
        DECREASE_ZOOM_ENABLED,
        INCREASE_ZOOM_ENABLED,
        DEFAULT_ZOOM_FACTOR,
        ZOOM_PERCENT_TEXT,
        RESET_ZOOM_CALLBACK
    };
}
