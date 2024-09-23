// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.chips;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties that describe a single chip in a list/group of chips. */
public class ChipProperties {
    /** ID for a basic chip in it's containing recycler view. */
    public static final int BASIC_CHIP = 0;

    /** An id to use for {@link #icon} when there is no icon on the chip. */
    public static final int INVALID_ICON_ID = ChipView.INVALID_ICON_ID;

    /** An id to use for the StringRes when we don't have a valid ID. */
    public static final int INVALID_STRING_RES_ID = -1;

    /** An value to use for {@link #textMaxWidthPx} when the whole text will be shown. */
    public static final int SHOW_WHOLE_TEXT = 0;

    /** A means of handling taps on a chip. The tapped chip model is provided in the callback. */
    public static final WritableObjectPropertyKey<Callback<PropertyModel>> CLICK_HANDLER =
            new WritableObjectPropertyKey<>();

    /** The description of the content inside the chip (for accessibility). */
    public static final WritableObjectPropertyKey<String> CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** Whether the chip is enabled (able to be tapped). */
    public static final WritableBooleanPropertyKey ENABLED = new WritableBooleanPropertyKey();

    /** An icon ID to show beside the chip's text. If no icon, use {@link #INVALID_ICON_ID}. */
    public static final WritableIntPropertyKey ICON = new WritableIntPropertyKey();

    /** Whether the icon should be tinted with text color. */
    public static final WritableBooleanPropertyKey APPLY_ICON_TINT =
            new WritableBooleanPropertyKey();

    /** An arbitrary ID for the chip to help identify it. */
    public static final ReadableIntPropertyKey ID = new ReadableIntPropertyKey();

    /** Whether the chip is currently selected (which also updates the color of the chip). */
    public static final WritableBooleanPropertyKey SELECTED = new WritableBooleanPropertyKey();

    /** The text that will be displayed inside the chip. */
    public static final ReadableObjectPropertyKey<String> TEXT = new ReadableObjectPropertyKey<>();

    /** The max width a chip's text should have in PX. Use {@link #SHOW_WHOLE_TEXT} for no limit. */
    public static final WritableIntPropertyKey TEXT_MAX_WIDTH_PX = new WritableIntPropertyKey();

    // Res id for the style to apply to the primary text view of the chip.
    public static final WritableIntPropertyKey PRIMARY_TEXT_APPEARANCE =
            new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                APPLY_ICON_TINT,
                CLICK_HANDLER,
                CONTENT_DESCRIPTION,
                ENABLED,
                ICON,
                ID,
                PRIMARY_TEXT_APPEARANCE,
                SELECTED,
                TEXT,
                TEXT_MAX_WIDTH_PX
            };
}
