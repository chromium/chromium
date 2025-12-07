// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

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
     * Select "Remove" to remove a Top Sites Tile.
     *
     * @param siteSuggestionsAfterRemove The site suggestions after remove.
     * @param fakeMostVisitedSites The fake most visited sites.
     * @return The new {@link MvtRemovedSnackbarFacility} after removal and the Undo Snackbar.
     */
    public MvtRemovedSnackbarFacility selectRemove(
            List<SiteSuggestion> siteSuggestionsAfterRemove,
            FakeMostVisitedSites fakeMostVisitedSites) {
        var mvtsAfterRemove = new MvtsFacility(siteSuggestionsAfterRemove);
        var snackbar = new MvtRemovedSnackbarFacility(mMvtsFacility, mvtsAfterRemove);
        runTo(
                        () -> {
                            invokeMenuItem("Remove");

                            ThreadUtils.runOnUiThreadBlocking(
                                    () ->
                                            fakeMostVisitedSites.setTileSuggestions(
                                                    siteSuggestionsAfterRemove));
                        })
                .exitFacilitiesAnd(this)
                .enterFacilities(mvtsAfterRemove, snackbar);
        return snackbar;
    }

    /**
     * Select "Unpin" to unpin a Custom Link Tile.
     *
     * @param siteSuggestionsAfterUnpin The site suggestions after unpin.
     * @param fakeMostVisitedSites The fake most visited sites.
     * @return The new {@link MvtUnpinnedSnackbarFacility} after unpin and the Undo Snackbar.
     */
    public MvtUnpinnedSnackbarFacility selectUnpin(
            List<SiteSuggestion> siteSuggestionsAfterUnpin,
            FakeMostVisitedSites fakeMostVisitedSites) {
        var mvtsAfterUnpin = new MvtsFacility(siteSuggestionsAfterUnpin);
        var snackbar = new MvtUnpinnedSnackbarFacility(mMvtsFacility, mvtsAfterUnpin);
        runTo(
                        () -> {
                            invokeMenuItem("Unpin");

                            ThreadUtils.runOnUiThreadBlocking(
                                    () ->
                                            fakeMostVisitedSites.setTileSuggestions(
                                                    siteSuggestionsAfterUnpin));
                        })
                .exitFacilitiesAnd(this)
                .enterFacilities(mvtsAfterUnpin, snackbar);
        return snackbar;
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

    /** Select "Open in incognito window" to open the tile in a new incognito window. */
    public WebPageStation selectOpenInIncognitoWindow() {
        String url = mMvtsTileFacility.getSiteSuggestion().url.getSpec();
        return invokeMenuItemTo("Open in Incognito window")
                .inNewTask()
                .arriveAt(
                        WebPageStation.newBuilder()
                                .withEntryPoint()
                                .withIncognito(true)
                                .withExpectedUrlSubstring(url)
                                .build());
    }
}
