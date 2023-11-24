// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Contains all the properties for each instance item listed in the switcher UI. */
public class InstanceSwitcherItemProperties {
    public static final PropertyModel.WritableBooleanPropertyKey CURRENT =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey ENABLE_COMMAND =
            new PropertyModel.WritableBooleanPropertyKey();

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

    public static final PropertyModel.WritableObjectPropertyKey<ListMenuButtonDelegate> MORE_MENU =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                CURRENT,
                ENABLE_COMMAND,
                FAVICON,
                TITLE,
                DESC,
                INSTANCE_ID,
                CLICK_LISTENER,
                MORE_MENU
            };
}
