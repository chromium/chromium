// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.content_public.browser.WebContents;

/**
 * Utilities for Toolbar unit tests.
 */
public class ToolbarUnitTestUtils {
    public static final LocationBarModel.OfflineStatus OFFLINE_STATUS =
            new LocationBarModel.OfflineStatus() {
                @Override
                public boolean isShowingTrustedOfflinePage(WebContents webContents) {
                    return true;
                }

                @Override
                public boolean isOfflinePage(Tab tab) {
                    return true;
                }
            };
}
