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
        int EXTENSION_ACTION = 0;
    }

    // Keep the entries sorted by name.

    /** The icon of the action. */
    public static final WritableObjectPropertyKey<Bitmap> ICON = new WritableObjectPropertyKey<>();

    /** The action ID (i.e. extension ID). */
    public static final WritableObjectPropertyKey<String> ID = new WritableObjectPropertyKey<>();

    /** The primary-click listener. */
    public static final WritableObjectPropertyKey<View.OnClickListener> ON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The context-click listener. */
    public static final WritableObjectPropertyKey<View.OnContextClickListener>
            ON_CONTEXT_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    /**
     * The title of the action. It is the name of the extension by default, but an extension can
     * update it programmatically.
     */
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    /** The list of all keys defined here. */
    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {ICON, ID, ON_CLICK_LISTENER, ON_CONTEXT_CLICK_LISTENER, TITLE};
}
