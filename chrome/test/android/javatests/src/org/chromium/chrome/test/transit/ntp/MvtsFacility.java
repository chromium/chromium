// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParentIndex;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.instanceOf;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.MoreViewConditions.ViewHasChildrenCountCondition;
import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.SuggestionsTileView;
import org.chromium.chrome.test.transit.page.WebPageStation;

import java.util.ArrayList;
import java.util.List;

/** Represents the Most Visited Tiles section in the New Tab Page. */
public class MvtsFacility extends ScrollableFacility<RegularNewTabPageStation> {

    public static final ViewSpec MOST_VISITED_TILES_LAYOUT = viewSpec(withId(R.id.mv_tiles_layout));

    private final List<SiteSuggestion> mSiteSuggestions;
    private ArrayList<Item<WebPageStation>> mTiles;

    /** Constructor. Expects the tiles to show the given |siteSuggestions|. */
    public MvtsFacility(List<SiteSuggestion> siteSuggestions) {
        mSiteSuggestions = siteSuggestions;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(RegularNewTabPageStation.MOST_VISITED_TILES_CONTAINER);
        // 1% visibility is enough because this layout is clipped by being inside scroll view in
        // tablets.
        Supplier<View> tilesViewSupplier =
                elements.declareView(
                        MOST_VISITED_TILES_LAYOUT, ViewElement.displayingAtLeastOption(1));
        elements.declareEnterCondition(
                new ViewHasChildrenCountCondition(tilesViewSupplier, mSiteSuggestions.size()));
        super.declareElements(elements);
    }

    @Override
    protected void declareItems(ScrollableFacility<RegularNewTabPageStation>.ItemsBuilder items) {
        mTiles = new ArrayList<>();
        for (int i = 0; i < mSiteSuggestions.size(); i++) {
            Matcher<View> tileMatcher =
                    allOf(instanceOf(SuggestionsTileView.class), withParentIndex(i));
            SiteSuggestion siteSuggestion = mSiteSuggestions.get(i);
            Item<WebPageStation> item =
                    items.declareItemToStation(
                            tileMatcher,
                            /* offScreenDataMatcher= */ null,
                            () ->
                                    WebPageStation.newBuilder()
                                            .withIncognito(false)
                                            .withIsOpeningTabs(0)
                                            .withTabAlreadySelected(mHostStation.getLoadedTab())
                                            .withExpectedUrlSubstring(siteSuggestion.url.getPath())
                                            .build());
            mTiles.add(item);
        }
    }

    @Override
    protected int getMinimumOnScreenItemCount() {
        // Number of tiles varies depending on screen size and variations, but should always
        // see at least 3 tiles.
        return 3;
    }

    /** Selects one of the tiles to navigate to that web page, scrolling if needed. */
    public WebPageStation scrollToAndSelectByIndex(int i) {
        assert i >= 0 && i < mSiteSuggestions.size();

        return mTiles.get(i).scrollToAndSelect();
    }
}
