// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Contains all the properties for each instance item listed in the switcher UI. */
@NullMarked
public class InstanceSwitcherItemProperties {
    public static final PropertyModel.WritableBooleanPropertyKey CURRENT =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey ENABLE_COMMAND =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableObjectPropertyKey<String> MAX_INFO_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<Drawable> FAVICON =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<String> TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<String> DESC =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableIntPropertyKey INSTANCE_ID =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            CLOSE_BUTTON_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String>
            CLOSE_BUTTON_CONTENT_DESCRIPTION = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey CLOSE_BUTTON_ENABLED =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableObjectPropertyKey<ListMenuDelegate> MORE_MENU =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String>
            MORE_MENU_CONTENT_DESCRIPTION = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey MORE_MENU_ENABLED =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableObjectPropertyKey<String> LAST_ACCESSED =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableBooleanPropertyKey IS_SELECTED =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                CURRENT,
                ENABLE_COMMAND,
                MAX_INFO_TEXT,
                FAVICON,
                TITLE,
                DESC,
                INSTANCE_ID,
                CLICK_LISTENER,
                CLOSE_BUTTON_CLICK_LISTENER,
                CLOSE_BUTTON_CONTENT_DESCRIPTION,
                CLOSE_BUTTON_ENABLED,
                LAST_ACCESSED,
                MORE_MENU,
                MORE_MENU_CONTENT_DESCRIPTION,
                MORE_MENU_ENABLED,
                IS_SELECTED
            };
}
