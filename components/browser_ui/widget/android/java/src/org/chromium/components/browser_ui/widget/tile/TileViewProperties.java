// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.browser_ui.widget.tile;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** TileView properties. */
public final class TileViewProperties {
    /** The title of the tile. */
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    /** Maximum number of lines used to present the title. */
    public static final WritableIntPropertyKey TITLE_LINES = new WritableIntPropertyKey();

    /** The primary icon used by the tile. */
    public static final WritableObjectPropertyKey<Drawable> ICON =
            new WritableObjectPropertyKey<>();

    /** The color state list to use to tint the embedded icon. Nullable. */
    public static final WritableObjectPropertyKey<ColorStateList> ICON_TINT =
            new WritableObjectPropertyKey<>();

    /** Whether Tile should present a large icon. */
    public static final WritableBooleanPropertyKey SHOW_LARGE_ICON =
            new WritableBooleanPropertyKey();

    /**
     * The rounding radius of the presented icon.
     * Applied only if the small icon is used. Large icons are always rounded to the view size
     * (circular)
     */
    public static final WritableIntPropertyKey SMALL_ICON_ROUNDING_RADIUS =
            new WritableIntPropertyKey();

    /** Badge visibility. */
    public static final WritableBooleanPropertyKey BADGE_VISIBLE = new WritableBooleanPropertyKey();

    /** Content description used by Accessibility to announce selection. */
    public static final WritableObjectPropertyKey<CharSequence> CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** Handler receiving focus events. */
    public static final WritableObjectPropertyKey<Runnable> ON_FOCUS_VIA_SELECTION =
            new WritableObjectPropertyKey<>();

    /** Handler receiving click events. */
    public static final WritableObjectPropertyKey<View.OnClickListener> ON_CLICK =
            new WritableObjectPropertyKey<>();

    /** Handler receiving long-click events. */
    public static final WritableObjectPropertyKey<View.OnLongClickListener> ON_LONG_CLICK =
            new WritableObjectPropertyKey<>();

    /** Handler receiving context menu call events. */
    public static final WritableObjectPropertyKey<View.OnCreateContextMenuListener>
            ON_CREATE_CONTEXT_MENU = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ICON,
                ICON_TINT,
                TITLE,
                TITLE_LINES,
                BADGE_VISIBLE,
                SHOW_LARGE_ICON,
                SMALL_ICON_ROUNDING_RADIUS,
                CONTENT_DESCRIPTION,
                ON_FOCUS_VIA_SELECTION,
                ON_CLICK,
                ON_LONG_CLICK,
                ON_CREATE_CONTEXT_MENU
            };
}
