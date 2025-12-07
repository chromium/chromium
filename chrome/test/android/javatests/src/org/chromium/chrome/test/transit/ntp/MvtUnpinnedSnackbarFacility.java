// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.test.transit.ui.SnackbarFacility;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;

/** Facility for the Undo Snackbar displayed when a Custom Link Tile is unpinned. */
public class MvtUnpinnedSnackbarFacility extends SnackbarFacility<RegularNewTabPageStation> {
    private final MvtsFacility mMvtsBeforeUnpin;
    private final MvtsFacility mMvtsAfterUnpin;

    public MvtUnpinnedSnackbarFacility(MvtsFacility mvtsBeforeUnpin, MvtsFacility mvtsAfterUnpin) {
        super("Shortcut unpinned", "Undo");
        mMvtsBeforeUnpin = mvtsBeforeUnpin;
        mMvtsAfterUnpin = mvtsAfterUnpin;
    }

    /** Click Undo to undo the tile unpin. */
    public MvtsFacility undo(FakeMostVisitedSites fakeMostVisitedSites) {
        var mvtsAfterUndo =
                new MvtsFacility(
                        mMvtsBeforeUnpin.getSiteSuggestions(),
                        mMvtsBeforeUnpin.getSeparatorIndices());
        return runTo(
                        () -> {
                            buttonElement.clickTo().executeTriggerWithoutTransition();
                            ThreadUtils.runOnUiThreadBlocking(
                                    () ->
                                            fakeMostVisitedSites.setTileSuggestions(
                                                    mMvtsBeforeUnpin.getSiteSuggestions()));
                        })
                .exitFacilitiesAnd(mMvtsAfterUnpin, this)
                .enterFacility(mvtsAfterUndo);
    }
}
