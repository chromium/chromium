// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.graphics.drawable.Drawable;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.Arrays;

/** {@link PropertyKey} list for app menu history items. */
@NullMarked
public class AppMenuHistoryItemProperties {
    /** The supplier for the icon for the menu item. */
    public static final WritableObjectPropertyKey<LazyOneshotSupplier<Drawable>> ICON_SUPPLIER =
            new WritableObjectPropertyKey<>("ICON_SUPPLIER");

    /** The recently closed entry associated with this item. */
    public static final WritableObjectPropertyKey<Object> RECENT_ENTRY =
            new WritableObjectPropertyKey<>("RECENT_ENTRY");

    public static final PropertyKey[] HISTORY_KEYS =
            new PropertyKey[] {ICON_SUPPLIER, RECENT_ENTRY};

    public static final PropertyKey[] ALL_KEYS =
            Arrays.copyOf(
                    AppMenuItemProperties.ALL_KEYS,
                    AppMenuItemProperties.ALL_KEYS.length + HISTORY_KEYS.length);

    static {
        for (int i = 0; i < HISTORY_KEYS.length; i++) {
            ALL_KEYS[ALL_KEYS.length - i - 1] = HISTORY_KEYS[i];
        }
    }
}
