// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParentIndex;

import static org.hamcrest.CoreMatchers.instanceOf;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import org.chromium.base.test.transit.MoreViewConditions.ViewHasChildrenCountCondition;
import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.SuggestionsTileView;
import org.chromium.chrome.test.transit.page.WebPageStation;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/** Represents the Most Visited Tiles section in the New Tab Page. */
public class MvtsFacility extends ScrollableFacility<RegularNewTabPageStation> {
    private final List<SiteSuggestion> mSiteSuggestions;
    private final Set<Integer> mNonTileIndices;
    public List<Item<WebPageStation>> tileItems;
    public ViewElement<View> tilesLayoutElement;

    /**
     * @param siteSuggestions List of expects the tiles to show.
     * @param nonTileIndices Set of tile container indices that are not tiles, e.g., dividers, or
     *     other UI that exist alongside tiles.
     */
    public MvtsFacility(List<SiteSuggestion> siteSuggestions, Set<Integer> nonTileIndices) {
        mSiteSuggestions = siteSuggestions;
        mNonTileIndices = nonTileIndices;
    }

    public MvtsFacility(List<SiteSuggestion> siteSuggestions) {
        this(siteSuggestions, Collections.emptySet());
    }

    @Override
    public void declareExtraElements() {
        // 1% visibility is enough because this layout is clipped by being inside scroll view in
        // tablets.
        tilesLayoutElement =
                declareView(withId(R.id.mv_tiles_layout), ViewElement.displayingAtLeastOption(1));
        declareEnterCondition(
                new ViewHasChildrenCountCondition(tilesLayoutElement, mSiteSuggestions.size()));

        // Will call declareItems()
        super.declareExtraElements();
    }

    @Override
    protected void declareItems(ScrollableFacility<RegularNewTabPageStation>.ItemsBuilder items) {
        int parentIndex = 0;
        ArrayList<Item<WebPageStation>> newTileItems = new ArrayList<>();
        for (int i = 0; i < mSiteSuggestions.size(); i++) {
            while (mNonTileIndices.contains(parentIndex)) {
                ++parentIndex;
            }
            ViewSpec<View> tileSpec =
                    viewSpec(instanceOf(SuggestionsTileView.class), withParentIndex(parentIndex));
            SiteSuggestion siteSuggestion = mSiteSuggestions.get(i);
            Item<WebPageStation> item =
                    items.declareItemToStation(
                            tileSpec,
                            /* offScreenDataMatcher= */ null,
                            () ->
                                    WebPageStation.newBuilder()
                                            .withIncognito(false)
                                            .withIsOpeningTabs(0)
                                            .withTabAlreadySelected(
                                                    mHostStation.loadedTabElement.get())
                                            .withExpectedUrlSubstring(siteSuggestion.url.getPath())
                                            .build());
            newTileItems.add(item);
            ++parentIndex;
        }
        tileItems = Collections.unmodifiableList(newTileItems);
    }

    @Override
    protected int getMinimumOnScreenItemCount() {
        // Number of tiles varies depending on screen size and variations, but should always
        // see at least 3 tiles.
        return 3;
    }
}
