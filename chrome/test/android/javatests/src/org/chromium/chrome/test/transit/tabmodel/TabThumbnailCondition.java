// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** Checks that a thumbnail was captured for a given tab. */
public class TabThumbnailCondition extends UiThreadCondition {
    private final TabModelSelector mTabModelSelector;
    private final Tab mTab;
    private final boolean mEtc1;

    private TabThumbnailCondition(TabModelSelector tabModelSelector, Tab tab, boolean etc1) {
        mTabModelSelector = tabModelSelector;
        mTab = tab;
        mEtc1 = etc1;
    }

    public static TabThumbnailCondition etc1(TabModelSelector selector, Tab tab) {
        return new TabThumbnailCondition(selector, tab, /* etc1= */ true);
    }

    public static TabThumbnailCondition jpeg(TabModelSelector selector, Tab tab) {
        return new TabThumbnailCondition(selector, tab, /* etc1= */ false);
    }

    @Override
    public ConditionStatus checkWithSuppliers() {
        if (mEtc1) {
            return whether(TabContentManager.getTabThumbnailFileEtc1(mTab).exists());
        } else {
            return whether(TabContentManager.getTabThumbnailFileJpeg(mTab.getId()).exists());
        }
    }

    @Override
    public String buildDescription() {
        return (mTab.isOffTheRecord() ? "Incognito" : "Regular")
                + " tab "
                + mTabModelSelector.getModel(mTab.isOffTheRecord()).indexOf(mTab)
                + (mEtc1 ? " etc1" : " jpeg")
                + " thumbnail cached to disk";
    }
}
