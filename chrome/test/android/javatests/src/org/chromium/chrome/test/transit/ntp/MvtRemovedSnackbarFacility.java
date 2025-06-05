// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.test.transit.ui.SnackbarFacility;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;

import java.util.List;

/** Facility for the Undo Snackbar displayed when a Most Visited Tiles tile is removed. */
public class MvtRemovedSnackbarFacility extends SnackbarFacility<RegularNewTabPageStation> {
    private final MvtsFacility mMvtsBeforeRemoval;
    private final MvtsFacility mMvtsAfterRemoval;

    public MvtRemovedSnackbarFacility(
            MvtsFacility mvtsBeforeRemoval, MvtsFacility mvtsAfterRemoval) {
        super("Item removed", "Undo");
        mMvtsBeforeRemoval = mvtsBeforeRemoval;
        mMvtsAfterRemoval = mvtsAfterRemoval;
    }

    /** Click Undo to undo the tile removal. */
    public MvtsFacility undo(FakeMostVisitedSites fakeMostVisitedSites) {
        var mvtsAfterUndo = new MvtsFacility(mMvtsBeforeRemoval.getSiteSuggestions());
        mHostStation.swapFacilitiesSync(
                List.of(mMvtsAfterRemoval, this),
                List.of(mvtsAfterUndo),
                () -> {
                    buttonElement.getClickTrigger().triggerTransition();
                    ThreadUtils.runOnUiThreadBlocking(
                            () ->
                                    fakeMostVisitedSites.setTileSuggestions(
                                            mMvtsBeforeRemoval.getSiteSuggestions()));
                });
        return mvtsAfterUndo;
    }
}
