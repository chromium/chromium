// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import android.util.Pair;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.SuggestionsTileView;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.ui.listmenu.ListMenuTestUtils;

import java.util.List;

/** Facility for a Most Visited Tiles tile displayed on the screen. */
public class MvtsTileFacility extends Facility<RegularNewTabPageStation> {
    private final MvtsFacility mMvtsFacility;
    private final int mIndex;
    private final @Nullable SiteSuggestion mSiteSuggestion;
    public final ViewElement<SuggestionsTileView> tileElement;

    public MvtsTileFacility(
            MvtsFacility mvtsFacility, int index, @Nullable SiteSuggestion siteSuggestion) {
        mMvtsFacility = mvtsFacility;
        mIndex = index;
        mSiteSuggestion = siteSuggestion;
        tileElement = declareView(MvtsFacility.createTileSpec(index));
    }

    /** Click the tile to navigate to its URL. */
    public WebPageStation clickToNavigateToWebPage() {
        assert mSiteSuggestion != null : String.format("Tile %d is not a site suggestion", mIndex);
        return tileElement
                .clickTo()
                .arriveAt(
                        WebPageStation.newBuilder()
                                .initFrom(mHostStation)
                                .withExpectedUrlSubstring(mSiteSuggestion.url.getPath())
                                .build());
    }

    /**
     * Open the tile context menu and select "Remove" to remove the tile.
     *
     * @param siteSuggestionsAfterRemoval The site suggestions after removal.
     * @param fakeMostVisitedSites The fake most visited sites.
     * @return The new {@link MvtsFacility} after removal and the Undo Snackbar.
     */
    public Pair<MvtsFacility, MvtRemovedSnackbarFacility> openContextMenuAndSelectRemove(
            List<SiteSuggestion> siteSuggestionsAfterRemoval,
            FakeMostVisitedSites fakeMostVisitedSites) {
        var mvtsAfterRemoval = new MvtsFacility(siteSuggestionsAfterRemoval);
        var snackbar = new MvtRemovedSnackbarFacility(mMvtsFacility, mvtsAfterRemoval);
        mHostStation.swapFacilitiesSync(
                List.of(mMvtsFacility, this),
                List.of(mvtsAfterRemoval, snackbar),
                () -> {
                    // TODO(crbug.com/420700079): Replace this with ListMenuFacility
                    ListMenuTestUtils.longClickAndWaitForListMenu(tileElement.get());
                    ListMenuTestUtils.invokeMenuItem("Remove");

                    ThreadUtils.runOnUiThreadBlocking(
                            () ->
                                    fakeMostVisitedSites.setTileSuggestions(
                                            siteSuggestionsAfterRemoval));
                });
        return Pair.create(mvtsAfterRemoval, snackbar);
    }
}
