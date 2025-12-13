// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.test.transit.ui.SnackbarFacility;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;

/** Facility for the Undo Snackbar displayed when a Top Sites Tile is removed. */
public class MvtRemovedSnackbarFacility extends SnackbarFacility<RegularNewTabPageStation> {
    private final MvtsFacility mMvtsBeforeRemoval;
    private final MvtsFacility mMvtsAfterRemoval;

    public MvtRemovedSnackbarFacility(
            MvtsFacility mvtsBeforeRemoval, MvtsFacility mvtsAfterRemoval) {
        super("This site won't be shown again", "Undo");
        mMvtsBeforeRemoval = mvtsBeforeRemoval;
        mMvtsAfterRemoval = mvtsAfterRemoval;
    }

    /** Click Undo to undo the tile removal. */
    public MvtsFacility undo(FakeMostVisitedSites fakeMostVisitedSites) {
        var mvtsAfterUndo =
                new MvtsFacility(
                        mMvtsBeforeRemoval.getSiteSuggestions(),
                        mMvtsBeforeRemoval.getSeparatorIndices());
        return runTo(
                        () -> {
                            buttonElement.clickTo().executeTriggerWithoutTransition();
                            ThreadUtils.runOnUiThreadBlocking(
                                    () ->
                                            fakeMostVisitedSites.setTileSuggestions(
                                                    mMvtsBeforeRemoval.getSiteSuggestions()));
                        })
                .exitFacilitiesAnd(mMvtsAfterRemoval, this)
                .enterFacility(mvtsAfterUndo);
    }
}
