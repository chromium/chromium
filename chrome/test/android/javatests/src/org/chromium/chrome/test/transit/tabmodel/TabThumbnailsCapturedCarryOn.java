// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.CarryOn;
import org.chromium.base.test.transit.Elements;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** CarryOn to check for existence of all tab thumbnails on disk for a TabModel. */
public class TabThumbnailsCapturedCarryOn extends CarryOn {
    private final boolean mIsIncognito;

    public TabThumbnailsCapturedCarryOn(boolean isIncognito) {
        mIsIncognito = isIncognito;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        Supplier<ChromeTabbedActivity> activitySupplier =
                elements.declareActivity(ChromeTabbedActivity.class);
        TabModelSelectorCondition tabModelSelectorCondition =
                elements.declareEnterCondition(new TabModelSelectorCondition(activitySupplier));
        elements.declareElementFactory(
                tabModelSelectorCondition,
                delayedElements -> {
                    TabModelSelector tabModelSelector = tabModelSelectorCondition.get();
                    TabModel tabModel = tabModelSelector.getModel(mIsIncognito);
                    int tabCount = tabModel.getCount();
                    for (int i = 0; i < tabCount; i++) {
                        delayedElements.declareEnterCondition(
                                TabThumbnailCondition.etc1(tabModelSelector, tabModel.getTabAt(i)));
                        delayedElements.declareEnterCondition(
                                TabThumbnailCondition.jpeg(tabModelSelector, tabModel.getTabAt(i)));
                    }
                });
    }
}
