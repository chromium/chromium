// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.graphics.Bitmap;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A set of extension action button properties to reflect its state. */
@NullMarked
public class ExtensionActionButtonProperties {
    @IntDef({ListItemType.EXTENSION_ACTION})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ListItemType {
        /** The draggable extension icon. */
        int EXTENSION_ACTION = 0;

        /** A non-visible placeholder item used for animations. */
        int ANCHOR = 1;
    }

    // Keep the entries sorted by name.

    /** The {@link ExtensionActionDragHelper} to distinguish input events. */
    public static final WritableObjectPropertyKey<ExtensionActionDragHelper> DRAG_HELPER =
            new WritableObjectPropertyKey<>();

    /** The icon of the action. */
    public static final WritableObjectPropertyKey<Bitmap> ICON = new WritableObjectPropertyKey<>();

    /** The action ID (i.e. extension ID). */
    public static final WritableObjectPropertyKey<String> ID = new WritableObjectPropertyKey<>();

    /** The primary-click listener. */
    public static final WritableObjectPropertyKey<View.OnClickListener> ON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The context-click listener. */
    public static final WritableObjectPropertyKey<View.OnLongClickListener> ON_LONG_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /**
     * The title of the action. It is the name of the extension by default, but an extension can
     * update it programmatically.
     */
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    /* The touch listener. */
    public static final WritableObjectPropertyKey<View.OnTouchListener> TOUCH_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The list of all keys defined here. */
    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                DRAG_HELPER,
                ICON,
                ID,
                ON_CLICK_LISTENER,
                ON_LONG_CLICK_LISTENER,
                TITLE,
                TOUCH_LISTENER
            };
}
