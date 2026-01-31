// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.content_settings.ContentSettingsType;

// TODO(crbug.com/463333225): Move dialogPosition into this class and rename it to
// PageInfoOpeningConfiguration.
/** Class for providing the page info highlight row information and opening configuration. */
@NullMarked
public class PageInfoHighlight {
    private final @ContentSettingsType.EnumType int mHighlightedPermission;
    private final boolean mOpenSubpage;

    public static PageInfoHighlight noHighlight() {
        return new PageInfoHighlight(
                PageInfoController.NO_HIGHLIGHTED_PERMISSION, /* openSubpage= */ false);
    }

    protected PageInfoHighlight(
            @ContentSettingsType.EnumType int highlightedPermission, boolean openSubpage) {
        mHighlightedPermission = highlightedPermission;
        mOpenSubpage = openSubpage;
    }

    public @ContentSettingsType.EnumType int getHighlightedPermission() {
        return mHighlightedPermission;
    }

    /**
     * @return Whether to automatically navigate to the permission subpage.
     */
    // TODO(crbug.com/463333225): Clean this provisional function name up if Clapper is launched or
    // removed.
    public boolean shouldOpenPermissionsSubpage() {
        return mOpenSubpage;
    }
}
