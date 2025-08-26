// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.test.transit.page.BasePageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;

/** Represents a non-grouped tab card in the Tab Switcher. */
public class TabSwitcherTabCardFacility extends TabSwitcherCardFacility {
    private final int mTabId;
    public ViewElement<View> closeButtonElement;

    public TabSwitcherTabCardFacility(@Nullable Integer cardIndex, int tabId, String title) {
        super(cardIndex, title);
        mTabId = tabId;
    }

    @Override
    public void declareExtraElements() {
        super.declareExtraElements();
        closeButtonElement = declareActionButton();
    }

    /** Clicks the tab card to show the page. */
    public <PageStationT extends CtaPageStation> PageStationT clickCard(
            BasePageStation.Builder<PageStationT> destinationBuilder) {
        boolean isSelecting =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mHostStation.getActivity().getActivityTab().getId() == mTabId);
        PageStationT destination =
                destinationBuilder
                        .withIncognito(mHostStation.isIncognito())
                        .withIsOpeningTabs(0)
                        .withIsSelectingTabs(isSelecting ? 1 : 0)
                        .withExpectedTitle(mTitle)
                        .build();
        return titleElement.clickTo().arriveAt(destination);
    }
}
