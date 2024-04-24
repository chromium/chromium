// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableLongPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties of message banner. */
public class MessageBannerProperties {
    /** A Color value indicating that the "natural" colors from the image should be used. */
    @ColorInt public static final int TINT_NONE = Color.TRANSPARENT;

    /**
     * The identifier for the message for recording metrics. It should be one of the values from
     * MessageIdentifier enum.
     */
    public static final ReadableIntPropertyKey MESSAGE_IDENTIFIER = new ReadableIntPropertyKey();

    /**
     * Controls the appearance of the primary widget, according to which value of the
     * PrimaryWidgetAppearance enum that this is set to. See the documentation of
     * PrimaryWidgetAppearance in components/messages/android/message_enums.h for details about each
     * possible value.
     */
    public static final WritableIntPropertyKey PRIMARY_WIDGET_APPEARANCE =
            new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<String> PRIMARY_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey PRIMARY_BUTTON_TEXT_MAX_LINES =
            new WritableIntPropertyKey();

    /**
     * See the documentation of PrimaryActionClickBehavior in
     * components/messages/android/message_enums.h for more information about the return value of
     * the primary action callback.
     */
    public static final WritableObjectPropertyKey<Supplier</*@PrimaryActionClickBehavior*/ Integer>>
            ON_PRIMARY_ACTION = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Runnable> ON_SECONDARY_ACTION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> TITLE_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<CharSequence> DESCRIPTION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Drawable> DESCRIPTION_ICON =
            new WritableObjectPropertyKey<>();

    /**
     * RESIZE_DESCRIPTION_ICON allows resizing the width of the drawable represented by
     * DESCRIPTION_ICON. This is useful when the icon width and height are unequal. If
     * RESIZE_DESCRIPTION_ICON is not specified, the DESCRIPTION_ICON will have a size of 18dp.
     */
    public static final WritableBooleanPropertyKey RESIZE_DESCRIPTION_ICON =
            new WritableBooleanPropertyKey();

    /**
     * DESCRIPTION_MAX_LINES allows limiting description view to the specified number of lines. The
     * description will be ellipsized with TruncateAt.END option.
     */
    public static final WritableIntPropertyKey DESCRIPTION_MAX_LINES = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<Drawable> ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey ICON_RESOURCE_ID = new WritableIntPropertyKey();
    // Large: 36 x 36dp; Default: 24dp (height) x wrap_content
    public static final WritableBooleanPropertyKey LARGE_ICON = new WritableBooleanPropertyKey();
    // Default: 0dp
    public static final WritableIntPropertyKey ICON_ROUNDED_CORNER_RADIUS_PX =
            new WritableIntPropertyKey();

    /**
     * If left unspecified, this will be default_icon_color_accent1. {@link #TINT_NONE} can be used
     * to completely remove the tint.
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
    public static final WritableObjectPropertyKey<ListMenuButtonDelegate>
            SECONDARY_MENU_BUTTON_DELEGATE = new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey SECONDARY_MENU_MAX_SIZE =
            new WritableIntPropertyKey();
    // Unit: milliseconds.
    public static final WritableLongPropertyKey DISMISSAL_DURATION = new WritableLongPropertyKey();

    /**
     * The callback invoked when the message is dismissed. DismissReason is passed through the
     * callback's parameter.
     */
    public static final WritableObjectPropertyKey<Callback<Integer>> ON_DISMISSED =
            new WritableObjectPropertyKey<>();

    /**
     * Whether the current message is in the foreground. True if the message is shown AND is in the
     * foreground when stacked. False if the message is hidden OR is in the background when stacked.
     */
    public static final WritableObjectPropertyKey<Callback<Boolean>> ON_FULLY_VISIBLE =
            new WritableObjectPropertyKey<>();

    // Following properties should only be accessed by the message banner component.
    static final WritableFloatPropertyKey TRANSLATION_X = new WritableFloatPropertyKey();
    static final WritableFloatPropertyKey TRANSLATION_Y = new WritableFloatPropertyKey();

    // Internal tracker of whether the message is in the foreground.
    static final WritableBooleanPropertyKey IS_FULLY_VISIBLE = new WritableBooleanPropertyKey();

    static final WritableIntPropertyKey MARGIN_TOP = new WritableIntPropertyKey();
    // ALPHA value of the content, i.e. every thing other than the background and shadow.
    static final WritableFloatPropertyKey CONTENT_ALPHA = new WritableFloatPropertyKey();
    // Height of the message for the expanding animation. This does not modify the view height
    // of the view in order to avoid triggering re-layout.
    static final WritableFloatPropertyKey VISUAL_HEIGHT = new WritableFloatPropertyKey();
    static final WritableObjectPropertyKey<Runnable> ON_TOUCH_RUNNABLE =
            new WritableObjectPropertyKey<>();
    static final WritableFloatPropertyKey ELEVATION = new WritableFloatPropertyKey();
    // PRIMARY_BUTTON_CLICK_LISTENER is SingleActionMessage's handler attached to primary button
    // view. SingleActionMessage calls ON_PRIMARY_ACTION from the handler.
    static final WritableObjectPropertyKey<OnClickListener> PRIMARY_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    // ON_SECONDARY_BUTTON_CLICK is SingleActionMessage's handler that calls ON_SECONDARY_ACTION.
    static final WritableObjectPropertyKey<Runnable> ON_SECONDARY_BUTTON_CLICK =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                MESSAGE_IDENTIFIER,
                PRIMARY_BUTTON_TEXT,
                PRIMARY_BUTTON_TEXT_MAX_LINES,
                PRIMARY_BUTTON_CLICK_LISTENER,
                TITLE,
                TITLE_CONTENT_DESCRIPTION,
                DESCRIPTION,
                DESCRIPTION_ICON,
                RESIZE_DESCRIPTION_ICON,
                DESCRIPTION_MAX_LINES,
                ICON,
                ICON_RESOURCE_ID,
                ICON_TINT_COLOR,
                LARGE_ICON,
                ICON_ROUNDED_CORNER_RADIUS_PX,
                SECONDARY_ICON,
                SECONDARY_ICON_RESOURCE_ID,
                SECONDARY_BUTTON_MENU_TEXT,
                ON_SECONDARY_BUTTON_CLICK,
                SECONDARY_ICON_CONTENT_DESCRIPTION,
                DISMISSAL_DURATION,
                TRANSLATION_X,
                TRANSLATION_Y,
                CONTENT_ALPHA,
                ON_TOUCH_RUNNABLE,
                ON_PRIMARY_ACTION,
                ON_SECONDARY_ACTION,
                ON_DISMISSED,
                ON_FULLY_VISIBLE,
                IS_FULLY_VISIBLE,
                SECONDARY_MENU_BUTTON_DELEGATE,
                SECONDARY_MENU_MAX_SIZE,
                PRIMARY_WIDGET_APPEARANCE,
                ELEVATION,
                MARGIN_TOP,
                VISUAL_HEIGHT
            };
}
