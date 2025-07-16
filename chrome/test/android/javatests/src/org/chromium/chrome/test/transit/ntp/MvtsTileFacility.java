// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.SuggestionsTileView;
import org.chromium.chrome.test.transit.page.WebPageStation;

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

    /** Open the context menu for the tile. */
    public MvtsTileContextMenuFacility openContextMenu() {
        return tileElement
                .longPressTo()
                .enterFacility(new MvtsTileContextMenuFacility(mMvtsFacility, this));
    }

    SiteSuggestion getSiteSuggestion() {
        return mSiteSuggestion;
    }
}
