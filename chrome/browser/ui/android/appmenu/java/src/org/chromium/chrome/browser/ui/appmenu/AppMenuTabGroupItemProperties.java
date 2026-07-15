// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.Arrays;

/** {@link PropertyKey} list for app menu tab group items. */
@NullMarked
public class AppMenuTabGroupItemProperties {
    public static final WritableObjectPropertyKey<Token> TAB_GROUP_ID =
            new WritableObjectPropertyKey<>("TAB_GROUP_ID");

    public static final PropertyKey[] TAB_GROUP_KEYS = new PropertyKey[] {TAB_GROUP_ID};

    public static final PropertyKey[] ALL_KEYS =
            Arrays.copyOf(
                    AppMenuItemProperties.ALL_KEYS,
                    AppMenuItemProperties.ALL_KEYS.length + TAB_GROUP_KEYS.length);

    static {
        for (int i = 0; i < TAB_GROUP_KEYS.length; i++) {
            ALL_KEYS[ALL_KEYS.length - i - 1] = TAB_GROUP_KEYS[i];
        }
    }
}
