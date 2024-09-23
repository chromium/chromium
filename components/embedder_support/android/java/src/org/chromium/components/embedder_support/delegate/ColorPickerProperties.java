// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class is to store the current color choices and all the information needed to paint the
 * view.
 */
public class ColorPickerProperties {
    public static final PropertyModel.WritableIntPropertyKey CHOSEN_COLOR =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableIntPropertyKey CHOSEN_SUGGESTION_INDEX =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.ReadableIntPropertyKey SUGGESTIONS_NUM_COLUMNS =
            new PropertyModel.ReadableIntPropertyKey();

    public static final PropertyModel.ReadableObjectPropertyKey<ModelListAdapter>
            SUGGESTIONS_ADAPTER = new PropertyModel.ReadableObjectPropertyKey<>();

    public static final PropertyModel.WritableBooleanPropertyKey IS_ADVANCED_VIEW =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.ReadableObjectPropertyKey<Callback<Integer>>
            CUSTOM_COLOR_PICKED_CALLBACK = new PropertyModel.ReadableObjectPropertyKey<>();

    public static final PropertyModel.ReadableObjectPropertyKey<Callback<Void>>
            VIEW_SWITCHED_CALLBACK = new PropertyModel.ReadableObjectPropertyKey<>();

    public static final PropertyModel.ReadableObjectPropertyKey<Callback<Boolean>>
            MAKE_CHOICE_CALLBACK = new PropertyModel.ReadableObjectPropertyKey<>();

    public static final PropertyModel.ReadableObjectPropertyKey<Callback<Integer>>
            DIALOG_DISMISSED_CALLBACK = new PropertyModel.ReadableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        CHOSEN_COLOR,
        CHOSEN_SUGGESTION_INDEX,
        SUGGESTIONS_NUM_COLUMNS,
        SUGGESTIONS_ADAPTER,
        IS_ADVANCED_VIEW,
        CUSTOM_COLOR_PICKED_CALLBACK,
        VIEW_SWITCHED_CALLBACK,
        MAKE_CHOICE_CALLBACK,
        DIALOG_DISMISSED_CALLBACK
    };
}
