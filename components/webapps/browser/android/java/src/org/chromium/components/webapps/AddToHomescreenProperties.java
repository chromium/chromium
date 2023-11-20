// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import android.graphics.Bitmap;
import android.util.Pair;
import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Contains the properties that an add-to-homescreen {@link PropertyModel} can have. */
public class AddToHomescreenProperties {
    public static final PropertyModel.WritableObjectPropertyKey<String> TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String> URL =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String> DESCRIPTION =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<Pair<Bitmap, Boolean>> ICON =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableIntPropertyKey TYPE =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey CAN_SUBMIT =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<OnClickListener> CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String> NATIVE_INSTALL_BUTTON_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableFloatPropertyKey NATIVE_APP_RATING =
            new PropertyModel.WritableFloatPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        TITLE,
        URL,
        DESCRIPTION,
        ICON,
        TYPE,
        CAN_SUBMIT,
        CLICK_LISTENER,
        NATIVE_INSTALL_BUTTON_TEXT,
        NATIVE_APP_RATING
    };
}
