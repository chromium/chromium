// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoHighlight;

// TODO(crbug.com/463333225): Move dialogPosition into this class and rename it to
// ChromePageInfoOpeningConfiguration.
/**
 * Chrome's customization of {@link PageInfoHighlight}. This class provides Chrome-specific
 * highlight info and opening configuration to PageInfoController.
 */
@NullMarked
public class ChromePageInfoHighlight extends PageInfoHighlight {
    private final boolean mHighlightStoreInfo;

    /** Creates a highlight object with no specific highlighting or opening action. */
    public static ChromePageInfoHighlight noHighlight() {
        return new ChromePageInfoHighlight(
                PageInfoController.NO_HIGHLIGHTED_PERMISSION,
                /* highlightStoreInfo= */ false,
                /* openSubpage= */ false);
    }

    /**
     * Creates a highlight object that highlights the permission row in the main Page Info view.
     *
     * @param highlightedPermission The permission to highlight.
     */
    public static ChromePageInfoHighlight highlightPermission(
            @ContentSettingsType.EnumType int highlightedPermission) {
        return new ChromePageInfoHighlight(
                highlightedPermission, /* highlightStoreInfo= */ false, /* openSubpage= */ false);
    }

    /**
     * @param highlightedPermission The permission for which to open the subpage. This permission
     *     will also be highlighted within the subpage.
     */
    public static ChromePageInfoHighlight openPermissionSubpage(
            @ContentSettingsType.EnumType int highlightedPermission) {
        return new ChromePageInfoHighlight(
                highlightedPermission, /* highlightStoreInfo= */ false, /* openSubpage= */ true);
    }

    /**
     * Creates a highlight object that highlights the Store Info row.
     *
     * @param highlightStoreInfo Whether to highlight the store info row.
     */
    public static ChromePageInfoHighlight forStoreInfo(boolean highlightStoreInfo) {
        return new ChromePageInfoHighlight(
                PageInfoController.NO_HIGHLIGHTED_PERMISSION,
                highlightStoreInfo,
                /* openSubpage= */ false);
    }

    private ChromePageInfoHighlight(
            @ContentSettingsType.EnumType int highlightedPermission,
            boolean highlightStoreInfo,
            boolean openSubpage) {
        super(highlightedPermission, openSubpage);
        mHighlightStoreInfo = highlightStoreInfo;
    }

    public boolean shouldHighlightStoreInfo() {
        return mHighlightStoreInfo;
    }
}
