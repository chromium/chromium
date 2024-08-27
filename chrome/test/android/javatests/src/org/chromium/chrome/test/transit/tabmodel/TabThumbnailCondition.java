// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;

/** Checks that a thumbnail was captured for a given tab. */
public class TabThumbnailCondition extends UiThreadCondition {
    private final TabModel mTabModel;
    private final int mIndex;
    private final boolean mEtc1;

    private TabThumbnailCondition(TabModel tabModel, int index, boolean etc1) {
        mTabModel = tabModel;
        mIndex = index;
        mEtc1 = etc1;
    }

    public static TabThumbnailCondition etc1(TabModel tabModel, int index) {
        return new TabThumbnailCondition(tabModel, index, /* etc1= */ true);
    }

    public static TabThumbnailCondition jpeg(TabModel tabModel, int index) {
        return new TabThumbnailCondition(tabModel, index, /* etc1= */ false);
    }

    @Override
    public ConditionStatus checkWithSuppliers() {
        Tab tab = mTabModel.getTabAt(mIndex);
        if (mEtc1) {
            return whether(TabContentManager.getTabThumbnailFileEtc1(tab).exists());
        } else {
            return whether(TabContentManager.getTabThumbnailFileJpeg(tab.getId()).exists());
        }
    }

    @Override
    public String buildDescription() {
        return (mTabModel.isOffTheRecord() ? "Incognito" : "Regular")
                + " tab "
                + mIndex
                + (mEtc1 ? " etc1" : " jpeg")
                + " thumbnail cached to disk";
    }
}
