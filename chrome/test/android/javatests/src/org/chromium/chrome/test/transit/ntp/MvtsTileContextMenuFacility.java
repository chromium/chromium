// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import android.util.Pair;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tabmodel.TabCountChangedCondition;
import org.chromium.chrome.test.transit.ui.ListMenuFacility;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;

import java.util.List;

/** Facility for a Most Visited Tiles tile's context menu. */
public class MvtsTileContextMenuFacility extends ListMenuFacility<RegularNewTabPageStation> {
    private final MvtsFacility mMvtsFacility;
    private final MvtsTileFacility mMvtsTileFacility;

    public MvtsTileContextMenuFacility(
            MvtsFacility mvtsFacility, MvtsTileFacility mvtsTileFacility) {
        super();
        mMvtsFacility = mvtsFacility;
        mMvtsTileFacility = mvtsTileFacility;
    }

    /**
     * Select "Remove" to remove the tile.
     *
     * @param siteSuggestionsAfterRemoval The site suggestions after removal.
     * @param fakeMostVisitedSites The fake most visited sites.
     * @return The new {@link MvtsFacility} after removal and the Undo Snackbar.
     */
    public Pair<MvtsFacility, MvtRemovedSnackbarFacility> selectRemove(
            List<SiteSuggestion> siteSuggestionsAfterRemoval,
            FakeMostVisitedSites fakeMostVisitedSites) {
        var mvtsAfterRemoval = new MvtsFacility(siteSuggestionsAfterRemoval);
        var snackbar = new MvtRemovedSnackbarFacility(mMvtsFacility, mvtsAfterRemoval);
        runTo(
                        () -> {
                            invokeMenuItem("Remove");

                            ThreadUtils.runOnUiThreadBlocking(
                                    () ->
                                            fakeMostVisitedSites.setTileSuggestions(
                                                    siteSuggestionsAfterRemoval));
                        })
                .exitFacilitiesAnd(mMvtsFacility, this)
                .enterFacilities(mvtsAfterRemoval, snackbar);
        return Pair.create(mvtsAfterRemoval, snackbar);
    }

    /** Select "Open in new tab" to open the tile in a new tab in background. */
    public void selectOpenInNewTab() {
        invokeMenuItemTo("Open in new tab")
                .waitForAnd(new TabCountChangedCondition(mHostStation.getTabModel(), +1))
                .exitFacility();
    }

    /** Select "Open in incognito tab" to open the tile in a new incognito tab. */
    public WebPageStation selectOpenInIncognitoTab() {
        String url = mMvtsTileFacility.getSiteSuggestion().url.getSpec();
        return invokeMenuItemTo("Open in Incognito tab")
                .arriveAt(
                        WebPageStation.newBuilder()
                                .initOpeningNewTab()
                                .withIncognito(true)
                                .withExpectedUrlSubstring(url)
                                .build());
    }
}
