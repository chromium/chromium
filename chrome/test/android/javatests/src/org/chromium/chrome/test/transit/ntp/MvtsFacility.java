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
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesLayout;
import org.chromium.chrome.browser.suggestions.tile.SuggestionsTileView;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Represents the Most Visited Tiles section in the New Tab Page. */
public class MvtsFacility extends ScrollableFacility<RegularNewTabPageStation> {
    private final List<SiteSuggestion> mSiteSuggestions;
    private List<@Nullable SiteSuggestion> mSiteSuggestionsByTileIndex;
    private final Set<Integer> mNonTileIndices;
    private final @Nullable Integer mAddNewButtonIndex;
    public ViewElement<MostVisitedTilesLayout> tilesLayoutElement;
    public List<Item> tileItems;
    public @Nullable Item addNewButtonItem;

    /**
     * @param siteSuggestions List of expects the tiles to show.
     * @param separatorIndices Set of tile container dividers.
     */
    public MvtsFacility(List<SiteSuggestion> siteSuggestions, Set<Integer> separatorIndices) {
        mSiteSuggestions = siteSuggestions;

        mNonTileIndices = new HashSet<>(separatorIndices);
        int childCount = siteSuggestions.size() + separatorIndices.size();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION)) {
            // Populate with the "Add new" button at the end.
            mAddNewButtonIndex = childCount;
            mNonTileIndices.add(mAddNewButtonIndex);
            childCount++;
        } else {
            mAddNewButtonIndex = null;
        }

        // 1% visibility is enough because this layout is clipped by being inside scroll view in
        // tablets.
        tilesLayoutElement =
                declareView(
                        MostVisitedTilesLayout.class,
                        withId(R.id.mv_tiles_layout),
                        ViewElement.displayingAtLeastOption(1));
        declareEnterCondition(new ViewHasChildrenCountCondition(tilesLayoutElement, childCount));
    }

    public MvtsFacility(List<SiteSuggestion> siteSuggestions) {
        this(siteSuggestions, Collections.emptySet());
    }

    @Override
    protected void declareItems(ScrollableFacility<RegularNewTabPageStation>.ItemsBuilder items) {
        mSiteSuggestionsByTileIndex = new ArrayList<>();
        int parentIndex = 0;
        ArrayList<Item> newTileItems = new ArrayList<>();
        for (int i = 0; i < mSiteSuggestions.size(); i++) {
            while (mNonTileIndices.contains(parentIndex)) {
                ++parentIndex;
            }

            mSiteSuggestionsByTileIndex.add(mSiteSuggestions.get(i));
            ViewSpec<? extends View> onScreenViewSpec = createTileSpec(parentIndex);
            /* offScreenDataMatcher= */ Item item = items.declareItem(onScreenViewSpec, null);
            newTileItems.add(item);
            ++parentIndex;
        }

        if (mAddNewButtonIndex != null) {
            mSiteSuggestionsByTileIndex.add(null);
            ViewSpec<? extends View> onScreenViewSpec = createTileSpec(mAddNewButtonIndex);
            /* offScreenDataMatcher= */ addNewButtonItem =
                    items.declareItem(onScreenViewSpec, null);
            newTileItems.add(addNewButtonItem);
        }
        tileItems = Collections.unmodifiableList(newTileItems);
    }

    static ViewSpec<SuggestionsTileView> createTileSpec(int parentIndex) {
        return viewSpec(
                SuggestionsTileView.class,
                instanceOf(SuggestionsTileView.class),
                withParentIndex(parentIndex));
    }

    @Override
    protected int getMinimumOnScreenItemCount() {
        // Number of tiles varies depending on screen size and variations, but should always
        // see at least 3 tiles.
        return 3;
    }

    List<SiteSuggestion> getSiteSuggestions() {
        return mSiteSuggestions;
    }

    Set<Integer> getSeparatorIndices() {
        Set<Integer> separators = new HashSet<>(mNonTileIndices);
        if (mAddNewButtonIndex != null) {
            separators.remove(mAddNewButtonIndex);
        }
        return separators;
    }

    /** Ensure tile with index |i| is visible, maybe scrolling to it. */
    public MvtsTileFacility ensureTileIsDisplayedAndGet(int i) {
        assert 0 <= i && i < tileItems.size()
                : String.format("%d is out of bounds [0, %d]", i, tileItems.size());

        tileItems.get(i).scrollToItemIfNeeded();

        SiteSuggestion siteSuggestion = mSiteSuggestionsByTileIndex.get(i);
        return noopTo().enterFacility(new MvtsTileFacility(this, i, siteSuggestion));
    }
}
