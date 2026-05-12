// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.Arrays;

/** {@link PropertyKey} list for app menu bookmark items. */
@NullMarked
public class AppMenuBookmarkItemProperties {
    public static final WritableObjectPropertyKey<BookmarkId> BOOKMARK_ID =
            new WritableObjectPropertyKey<>("BOOKMARK_ID");

    public static final PropertyKey[] ALL_KEYS =
            Arrays.copyOf(
                    AppMenuItemProperties.ALL_KEYS, AppMenuItemProperties.ALL_KEYS.length + 1);

    static {
        ALL_KEYS[ALL_KEYS.length - 1] = BOOKMARK_ID;
    }
}
