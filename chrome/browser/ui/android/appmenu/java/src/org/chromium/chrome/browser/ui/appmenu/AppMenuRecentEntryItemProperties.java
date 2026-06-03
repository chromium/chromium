// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.Arrays;

/** {@link PropertyKey} list for app menu recent entry items. */
@NullMarked
public class AppMenuRecentEntryItemProperties {
    /** The recently closed entry associated with this item. */
    public static final WritableObjectPropertyKey<Object> RECENT_ENTRY =
            new WritableObjectPropertyKey<>("RECENT_ENTRY");

    public static final PropertyKey[] RECENT_ENTRY_KEYS = new PropertyKey[] {RECENT_ENTRY};

    public static final PropertyKey[] ALL_KEYS =
            Arrays.copyOf(
                    AppMenuItemProperties.ALL_KEYS,
                    AppMenuItemProperties.ALL_KEYS.length + RECENT_ENTRY_KEYS.length);

    static {
        for (int i = 0; i < RECENT_ENTRY_KEYS.length; i++) {
            ALL_KEYS[ALL_KEYS.length - i - 1] = RECENT_ENTRY_KEYS[i];
        }
    }
}
