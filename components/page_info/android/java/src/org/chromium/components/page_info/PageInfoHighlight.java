// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import org.chromium.components.content_settings.ContentSettingsType;

/** Class for providing the page info highlight row information. */
public class PageInfoHighlight {
    private final @ContentSettingsType.EnumType int mHighlightedPermission;

    public static PageInfoHighlight noHighlight() {
        return new PageInfoHighlight(PageInfoController.NO_HIGHLIGHTED_PERMISSION);
    }

    protected PageInfoHighlight(@ContentSettingsType.EnumType int highlightedPermission) {
        mHighlightedPermission = highlightedPermission;
    }

    public @ContentSettingsType.EnumType int getHighlightedPermission() {
        return mHighlightedPermission;
    }
}
