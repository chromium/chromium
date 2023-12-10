// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import android.content.res.ColorStateList;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.function.BooleanSupplier;

class OptionalButtonProperties {
    // We skip equality checks because some controllers update their button by changing the
    // ButtonSpec value on the same ButtonData instance. In addition we don't split this into
    // BUTTON_SPEC and CAN_SHOW because it would be hard to avoid two animations when both the spec
    // and visibility change at the same time.
    public static final WritableObjectPropertyKey<ButtonData> BUTTON_DATA =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true);
    public static final WritableBooleanPropertyKey IS_ENABLED = new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<Callback<Integer>> TRANSITION_STARTED_CALLBACK =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Callback<Integer>> TRANSITION_FINISHED_CALLBACK =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> ON_BEFORE_HIDE_TRANSITION_CALLBACK =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<ViewGroup> TRANSITION_ROOT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<ColorStateList> ICON_TINT_LIST =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey ICON_BACKGROUND_COLOR = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey ICON_BACKGROUND_ALPHA = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey PADDING_START = new WritableIntPropertyKey();
    public static final WritableBooleanPropertyKey TRANSITION_CANCELLATION_REQUESTED =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<BooleanSupplier> IS_ANIMATION_ALLOWED_PREDICATE =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        BUTTON_DATA,
        IS_ENABLED,
        TRANSITION_STARTED_CALLBACK,
        TRANSITION_FINISHED_CALLBACK,
        ON_BEFORE_HIDE_TRANSITION_CALLBACK,
        TRANSITION_ROOT,
        ICON_TINT_LIST,
        ICON_BACKGROUND_COLOR,
        ICON_BACKGROUND_ALPHA,
        PADDING_START,
        TRANSITION_CANCELLATION_REQUESTED,
        IS_ANIMATION_ALLOWED_PREDICATE
    };
}
