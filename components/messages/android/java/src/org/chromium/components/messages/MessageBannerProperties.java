// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Properties of message banner.
 */
public class MessageBannerProperties {
    public static final WritableObjectPropertyKey<String> PRIMARY_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> ON_PRIMARY_ACTION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> ON_SECONDARY_ACTION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> DESCRIPTION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Drawable> ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey ICON_RESOURCE_ID = new WritableIntPropertyKey();
    // Secondary icon is shown as a button, so content description should be always set.
    public static final WritableObjectPropertyKey<Drawable> SECONDARY_ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey SECONDARY_ICON_RESOURCE_ID =
            new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<String> SECONDARY_ACTION_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> SECONDARY_ICON_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();
    // TODO(crbug.com/1123947): remove this since on_dismissed is not a property of the view?
    public static final WritableObjectPropertyKey<Runnable> ON_DISMISSED =
            new WritableObjectPropertyKey<>();

    // Following properties should only be accessed by the message banner component.
    static final WritableFloatPropertyKey TRANSLATION_Y = new WritableFloatPropertyKey();
    static final WritableFloatPropertyKey ALPHA = new WritableFloatPropertyKey();
    static final WritableObjectPropertyKey<Runnable> ON_TOUCH_RUNNABLE =
            new WritableObjectPropertyKey<>();
    // PRIMARY_BUTTON_CLICK_LISTENER is SingleActionMessage's handler attached to primary button
    // view. SingleActionMessage calls ON_PRIMARY_ACTION from the handler.
    static final WritableObjectPropertyKey<OnClickListener> PRIMARY_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    // TODO(pavely): There is no need to maintain two lists of property keys. Remove one and clean
    // up references.
    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {PRIMARY_BUTTON_TEXT, PRIMARY_BUTTON_CLICK_LISTENER, TITLE,
                    DESCRIPTION, ICON, ICON_RESOURCE_ID, SECONDARY_ICON, SECONDARY_ICON_RESOURCE_ID,
                    SECONDARY_ACTION_TEXT, SECONDARY_ICON_CONTENT_DESCRIPTION, TRANSLATION_Y, ALPHA,
                    ON_TOUCH_RUNNABLE, ON_PRIMARY_ACTION, ON_SECONDARY_ACTION};

    public static final PropertyKey[] SINGLE_ACTION_MESSAGE_KEYS =
            new PropertyKey[] {PRIMARY_BUTTON_TEXT, PRIMARY_BUTTON_CLICK_LISTENER, TITLE,
                    DESCRIPTION, ICON, ICON_RESOURCE_ID, SECONDARY_ICON, SECONDARY_ICON_RESOURCE_ID,
                    SECONDARY_ACTION_TEXT, ON_DISMISSED, TRANSLATION_Y, ALPHA, ON_TOUCH_RUNNABLE,
                    ON_PRIMARY_ACTION, ON_SECONDARY_ACTION};
}
