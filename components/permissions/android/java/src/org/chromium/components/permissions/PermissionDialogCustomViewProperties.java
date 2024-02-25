// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;

import androidx.core.util.Pair;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * The class is responsible for specifying the various properties of the Permission Dialog's custom
 * view.
 */
public class PermissionDialogCustomViewProperties {
    public static final PropertyModel.WritableObjectPropertyKey<Drawable> ICON =
            new PropertyModel.WritableObjectPropertyKey();

    public static final PropertyModel.WritableObjectPropertyKey<ColorStateList> ICON_TINT =
            new PropertyModel.WritableObjectPropertyKey();

    public static final PropertyModel.ReadableObjectPropertyKey<String> MESSAGE_TEXT =
            new PropertyModel.ReadableObjectPropertyKey();
    public static final PropertyModel.ReadableObjectPropertyKey<List<Pair<Integer, Integer>>>
            BOLDED_RANGES = new PropertyModel.ReadableObjectPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {ICON, ICON_TINT, MESSAGE_TEXT, BOLDED_RANGES};
}
