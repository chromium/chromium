// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.GURL;

import java.util.Arrays;

/** {@link PropertyKey} list for app menu tab items. */
@NullMarked
public class AppMenuTabItemProperties {
    public static final WritableIntPropertyKey TAB_ID = new WritableIntPropertyKey("TAB_ID");
    public static final WritableObjectPropertyKey<@Nullable GURL> TAB_URL =
            new WritableObjectPropertyKey<>("TAB_URL");

    public static final PropertyKey[] TAB_KEYS = new PropertyKey[] {TAB_ID, TAB_URL};

    public static final PropertyKey[] ALL_KEYS =
            Arrays.copyOf(
                    AppMenuItemProperties.ALL_KEYS,
                    AppMenuItemProperties.ALL_KEYS.length + TAB_KEYS.length);

    static {
        for (int i = 0; i < TAB_KEYS.length; i++) {
            ALL_KEYS[ALL_KEYS.length - i - 1] = TAB_KEYS[i];
        }
    }
}
