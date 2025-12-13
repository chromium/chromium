// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.Station;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.transit.tabmodel.TabGroupModelFilterCondition;
import org.chromium.chrome.test.transit.tabmodel.TabModelCondition;
import org.chromium.chrome.test.transit.tabmodel.TabModelSelectorCondition;

/** Base class for |Station<ChromeActivity>| that are related to a specific TabModel. */
public class ChromeActivityTabModelBoundStation<HostActivity extends ChromeActivity>
        extends Station<HostActivity> {
    protected final boolean mIsIncognito;

    public final Element<TabModelSelector> tabModelSelectorElement;
    public final Element<TabModel> tabModelElement;
    public final Element<TabGroupModelFilter> tabGroupModelFilterElement;

    public ChromeActivityTabModelBoundStation(
            Class<HostActivity> activityClass, boolean isIncognito) {
        super(activityClass);
        mIsIncognito = isIncognito;
        tabModelSelectorElement =
                declareEnterConditionAsElement(new TabModelSelectorCondition(mActivityElement));
        tabModelElement =
                declareEnterConditionAsElement(
                        new TabModelCondition(tabModelSelectorElement, mIsIncognito));
        tabGroupModelFilterElement =
                declareEnterConditionAsElement(
                        new TabGroupModelFilterCondition(tabModelSelectorElement, mIsIncognito));
    }

    /** Convenience method for |tabModelElement.get()|. */
    public TabModel getTabModel() {
        return tabModelElement.value();
    }

    /** Convenience method for |tabModelSelectorElement.get()|. */
    public TabModelSelector getTabModelSelector() {
        return tabModelSelectorElement.value();
    }

    /** Convenience method for |tabGroupModelFilterElement.get()|. */
    public TabGroupModelFilter getTabGroupModelFilter() {
        return tabGroupModelFilterElement.value();
    }

    /** Whether bound to an incognito TabModel. */
    public boolean isIncognito() {
        return mIsIncognito;
    }
}
