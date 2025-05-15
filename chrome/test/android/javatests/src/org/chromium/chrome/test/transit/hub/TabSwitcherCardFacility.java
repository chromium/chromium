// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.tasks.tab_management.TabGridView;
import org.chromium.chrome.test.R;

/** Base class for Card Facilities in the Tab Switcher. */
public abstract class TabSwitcherCardFacility extends Facility<TabSwitcherStation> {
    private final @Nullable Integer mCardIndex;
    protected final String mTitle;

    public ViewElement<View> titleElement;
    public ViewElement<View> cardViewElement;

    TabSwitcherCardFacility(@Nullable Integer cardIndex, String title) {
        mCardIndex = cardIndex;
        mTitle = title;
    }

    @Override
    @CallSuper
    public void declareExtraElements() {
        Matcher<View> cardTitleMatcher =
                allOf(withText(mTitle), withId(R.id.tab_title), withParent(withId(R.id.card_view)));
        titleElement = declareView(cardTitleMatcher);

        ViewSpec<View> cardSpec =
                viewSpec(isAssignableFrom(TabGridView.class), hasDescendant(cardTitleMatcher));
        cardViewElement = declareView(cardSpec);

        if (mCardIndex != null) {
            declareEnterCondition(
                    new CardAtPositionCondition(
                            mCardIndex, mHostStation.recyclerViewElement, cardViewElement));
        }
    }

    protected ViewElement<View> declareActionButton() {
        return declareView(cardViewElement.descendant(withId(R.id.action_button)));
    }
}
