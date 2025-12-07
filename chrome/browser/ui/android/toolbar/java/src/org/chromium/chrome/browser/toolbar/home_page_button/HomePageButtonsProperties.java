// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import android.content.res.ColorStateList;
import android.view.View;

import androidx.core.util.Pair;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class HomePageButtonsProperties {
    public static final PropertyModel.WritableIntPropertyKey CONTAINER_VISIBILITY =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableIntPropertyKey ACCESSIBILITY_TRAVERSAL_BEFORE =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableFloatPropertyKey TRANSLATION_Y =
            new PropertyModel.WritableFloatPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey IS_CLICKABLE =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableObjectPropertyKey<View.OnKeyListener>
            ON_KEY_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    // The key manages the visibility of home page buttons, with the integer representing the
    // button's index and the boolean indicating its visibility.
    public static final PropertyModel.WritableObjectPropertyKey<Pair<Integer, Boolean>>
            IS_BUTTON_VISIBLE = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<Pair<Integer, HomePageButtonData>>
            BUTTON_DATA = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<ColorStateList> BUTTON_TINT_LIST =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableIntPropertyKey BUTTON_BACKGROUND =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                CONTAINER_VISIBILITY,
                ACCESSIBILITY_TRAVERSAL_BEFORE,
                TRANSLATION_Y,
                IS_CLICKABLE,
                ON_KEY_LISTENER,
                IS_BUTTON_VISIBLE,
                BUTTON_DATA,
                BUTTON_TINT_LIST,
                BUTTON_BACKGROUND
            };
}
