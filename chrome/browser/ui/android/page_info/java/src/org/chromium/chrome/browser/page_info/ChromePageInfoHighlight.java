// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoHighlight;

/**
 * Chrome's customization of {@link PageInfoHighlight}. This class provides Chrome-specific
 * highlight info to PageInfoController.
 */
public class ChromePageInfoHighlight extends PageInfoHighlight {
    private final boolean mHighlightStoreInfo;

    public static ChromePageInfoHighlight noHighlight() {
        return new ChromePageInfoHighlight(PageInfoController.NO_HIGHLIGHTED_PERMISSION, false);
    }

    public static ChromePageInfoHighlight forPermission(
            @ContentSettingsType.EnumType int highlightedPermission) {
        return new ChromePageInfoHighlight(highlightedPermission, false);
    }

    public static ChromePageInfoHighlight forStoreInfo(boolean highlightStoreInfo) {
        return new ChromePageInfoHighlight(
                PageInfoController.NO_HIGHLIGHTED_PERMISSION, highlightStoreInfo);
    }

    private ChromePageInfoHighlight(
            @ContentSettingsType.EnumType int highlightedPermission, boolean highlightStoreInfo) {
        super(highlightedPermission);
        mHighlightStoreInfo = highlightStoreInfo;
    }

    public boolean shouldHighlightStoreInfo() {
        return mHighlightStoreInfo;
    }
}
