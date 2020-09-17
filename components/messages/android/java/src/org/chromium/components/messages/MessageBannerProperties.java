// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Properties of message banner.
 */
public class MessageBannerProperties {
    public static final WritableObjectPropertyKey<String> PRIMARY_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<OnClickListener> PRIMARY_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> DESCRIPTION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Drawable> ICON =
            new WritableObjectPropertyKey<>();
    // Secondary icon is shown as a button, so content description should be always set.
    public static final WritableObjectPropertyKey<Drawable> SECONDARY_ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> SECONDARY_ICON_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();
    // TODO(crbug.com/1123947): remove this since on_dismissed is not a property of the view?
    public static final WritableObjectPropertyKey<Runnable> ON_DISMISSED =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {PRIMARY_BUTTON_TEXT, PRIMARY_BUTTON_CLICK_LISTENER, TITLE,
                    DESCRIPTION, ICON, SECONDARY_ICON, SECONDARY_ICON_CONTENT_DESCRIPTION};

    public static final PropertyKey[] SINGLE_ACTION_MESSAGE_KEYS =
            new PropertyKey[] {PRIMARY_BUTTON_TEXT, PRIMARY_BUTTON_CLICK_LISTENER, TITLE,
                    DESCRIPTION, ICON, ON_DISMISSED};
}
