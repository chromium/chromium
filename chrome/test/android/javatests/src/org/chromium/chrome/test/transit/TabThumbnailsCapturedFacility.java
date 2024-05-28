// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

// TODO(b/328451039): switch to a CarryOn from Facility.
/** Facility to check for existence of tab thumbnails on disk. */
public class TabThumbnailsCapturedFacility extends Facility<Station> {

    private static class TabThumbnailCondition extends UiThreadCondition {
        private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
        private final boolean mIncognito;
        private final String mTabType;

        public TabThumbnailCondition(
                Supplier<TabModelSelector> tabModelSelectorSupplier, boolean incognito) {
            mIncognito = incognito;
            mTabModelSelectorSupplier =
                    dependOnSupplier(tabModelSelectorSupplier, "TabModelSelector");
            mTabType = incognito ? "Incognito" : "Regular";
        }

        @Override
        public ConditionStatus checkWithSuppliers() {
            TabModel tabModel = mTabModelSelectorSupplier.get().getModel(mIncognito);
            int tabCount = tabModel.getCount();
            StringBuilder tabThumbnailsMissing = null;
            for (int i = 0; i < tabCount; i++) {
                boolean etc1FileExists =
                        TabContentManager.getTabThumbnailFileEtc1(tabModel.getTabAt(i)).exists();
                boolean jpegFileExists =
                        TabContentManager.getTabThumbnailFileJpeg(tabModel.getTabAt(i).getId())
                                .exists();

                if (!etc1FileExists || !jpegFileExists) {
                    if (tabThumbnailsMissing == null) {
                        tabThumbnailsMissing =
                                new StringBuilder(
                                        "Waiting for thumbnails of ("
                                                + tabCount
                                                + ") tabs, missing: ");
                    } else {
                        tabThumbnailsMissing.append(", ");
                    }
                    tabThumbnailsMissing.append(i);
                }
            }
            if (tabThumbnailsMissing == null) {
                return fulfilled("All " + mTabType + " tab thumbnails on disk");
            }
            return awaiting(tabThumbnailsMissing.toString());
        }

        @Override
        public String buildDescription() {
            return mTabType + " tab thumbnails cached to disk";
        }
    }

    private boolean mIsIncognito;

    public TabThumbnailsCapturedFacility(Station station, boolean isIncognito) {
        super(station);
        mIsIncognito = isIncognito;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        Supplier<ChromeTabbedActivity> activitySupplier =
                elements.declareActivity(ChromeTabbedActivity.class);
        Supplier<TabModelSelector> tabModelSelectorSupplier =
                elements.declareEnterCondition(new TabModelSelectorCondition(activitySupplier));
        elements.declareEnterCondition(
                new TabThumbnailCondition(tabModelSelectorSupplier, mIsIncognito));
    }

    /** Waits until all tab thumbnails are captured to disk. */
    public static TabThumbnailsCapturedFacility waitForThumbnailsCaptured(
            Station currentStation, boolean isIncognito) {
        TabThumbnailsCapturedFacility thumbnailsCached =
                new TabThumbnailsCapturedFacility(currentStation, isIncognito);
        return currentStation.enterFacilitySync(thumbnailsCached, null);
    }
}
