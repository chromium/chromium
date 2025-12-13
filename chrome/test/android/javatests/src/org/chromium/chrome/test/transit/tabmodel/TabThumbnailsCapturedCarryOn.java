// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import org.chromium.base.test.transit.CarryOn;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** CarryOn to check for existence of all tab thumbnails on disk for a TabModel. */
public class TabThumbnailsCapturedCarryOn extends CarryOn {
    public TabThumbnailsCapturedCarryOn(TabModelSelector tabModelSelector, boolean isIncognito) {
        TabModel tabModel = tabModelSelector.getModel(isIncognito);
        int tabCount = runOnUiThreadBlocking(() -> tabModel.getCount());
        for (int i = 0; i < tabCount; i++) {
            int j = i; // Effectively final for the lambda.
            Tab tab = runOnUiThreadBlocking(() -> tabModel.getTabAt(j));
            declareEnterCondition(TabThumbnailCondition.etc1(tabModelSelector, tab));
            declareEnterCondition(TabThumbnailCondition.jpeg(tabModelSelector, tab));
        }
    }
}
