// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableLongPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Properties of message banner.
 */
public class MessageBannerProperties {
    /** A Color value indicating that the "natural" colors from the image should be used. */
    @ColorInt
    public static final int TINT_NONE = Color.TRANSPARENT;

    /**
     * The identifier for the message for recording metrics. It should be one of the values from
     * MessageIdentifier enum.
     */
    public static final ReadableIntPropertyKey MESSAGE_IDENTIFIER = new ReadableIntPropertyKey();
    public static final WritableObjectPropertyKey<String> PRIMARY_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> ON_PRIMARY_ACTION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> ON_SECONDARY_ACTION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<CharSequence> DESCRIPTION =
            new WritableObjectPropertyKey<>();
    /**
     * DESCRIPTION_MAX_LINES allows limiting description view to the specified number of lines. The
     * description will be ellipsized with TruncateAt.END option.
     */
    public static final WritableIntPropertyKey DESCRIPTION_MAX_LINES = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<Drawable> ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey ICON_RESOURCE_ID = new WritableIntPropertyKey();

    /**
     * If left unspecified, this will be default_icon_color_blue. {@link #TINT_NONE} can be used to
     * completely remove the tint.
     */
    public static final WritableIntPropertyKey ICON_TINT_COLOR = new WritableIntPropertyKey();
    // Secondary icon is shown as a button, so content description should be always set.
    public static final WritableObjectPropertyKey<Drawable> SECONDARY_ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey SECONDARY_ICON_RESOURCE_ID =
            new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<String> SECONDARY_BUTTON_MENU_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> SECONDARY_ICON_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();
    // Unit: milliseconds.
    public static final WritableLongPropertyKey DISMISSAL_DURATION = new WritableLongPropertyKey();
    /**
     * The callback invoked when the message is dismissed. DismissReason is passed through the
     * callback's parameter.
     */
    public static final WritableObjectPropertyKey<Callback<Integer>> ON_DISMISSED =
            new WritableObjectPropertyKey<>();

    // Following properties should only be accessed by the message banner component.
    static final WritableFloatPropertyKey TRANSLATION_X = new WritableFloatPropertyKey();
    static final WritableFloatPropertyKey TRANSLATION_Y = new WritableFloatPropertyKey();
    static final WritableFloatPropertyKey ALPHA = new WritableFloatPropertyKey();
    static final WritableObjectPropertyKey<Runnable> ON_TOUCH_RUNNABLE =
            new WritableObjectPropertyKey<>();
    // PRIMARY_BUTTON_CLICK_LISTENER is SingleActionMessage's handler attached to primary button
    // view. SingleActionMessage calls ON_PRIMARY_ACTION from the handler.
    static final WritableObjectPropertyKey<OnClickListener> PRIMARY_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {MESSAGE_IDENTIFIER,
            PRIMARY_BUTTON_TEXT, PRIMARY_BUTTON_CLICK_LISTENER, TITLE, DESCRIPTION,
            DESCRIPTION_MAX_LINES, ICON, ICON_RESOURCE_ID, ICON_TINT_COLOR, SECONDARY_ICON,
            SECONDARY_ICON_RESOURCE_ID, SECONDARY_BUTTON_MENU_TEXT,
            SECONDARY_ICON_CONTENT_DESCRIPTION, DISMISSAL_DURATION, TRANSLATION_X, TRANSLATION_Y,
            ALPHA, ON_TOUCH_RUNNABLE, ON_PRIMARY_ACTION, ON_SECONDARY_ACTION, ON_DISMISSED};
}
