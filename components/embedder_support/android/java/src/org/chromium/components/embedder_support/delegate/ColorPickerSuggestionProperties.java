// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** This class is to store everything needed for the suggestion view */
public class ColorPickerSuggestionProperties {
    @IntDef({ListItemType.DEFAULT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ListItemType {
        int DEFAULT = 0; // There is only one type of color suggestion view.
    }

    public static final PropertyModel.ReadableIntPropertyKey INDEX =
            new PropertyModel.ReadableIntPropertyKey();

    public static final PropertyModel.ReadableIntPropertyKey COLOR =
            new PropertyModel.ReadableIntPropertyKey();

    public static final PropertyModel.ReadableObjectPropertyKey<String> LABEL =
            new PropertyModel.ReadableObjectPropertyKey<>();

    public static final PropertyModel.WritableBooleanPropertyKey IS_SELECTED =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.ReadableObjectPropertyKey<Callback<Integer>> ONCLICK =
            new PropertyModel.ReadableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {INDEX, COLOR, LABEL, IS_SELECTED, ONCLICK};
}
