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

import static org.chromium.base.test.transit.ViewElement.elementIdOption;
import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.tasks.tab_management.TabGridView;
import org.chromium.chrome.test.R;

/** Base class for Card Facilities in the Tab Switcher. */
public abstract class TabSwitcherCardFacility extends Facility<TabSwitcherStation> {
    public static final Matcher<View> CARD_MATCHER = withId(R.id.card_view);
    private final @Nullable Integer mCardIndex;
    protected final String mTitle;

    private ViewSpec mCardTitleSpec;

    TabSwitcherCardFacility(@Nullable Integer cardIndex, String title) {
        mCardIndex = cardIndex;
        mTitle = title;
    }

    @Override
    @CallSuper
    public void declareElements(Elements.Builder elements) {
        String titleElementId = "Card title: " + mTitle;
        Matcher<View> cardTitleMatcher = cardTitleMatcher(mTitle);
        mCardTitleSpec = viewSpec(cardTitleMatcher);
        elements.declareView(mCardTitleSpec, elementIdOption(titleElementId));

        ViewSpec cardSpec =
                viewSpec(isAssignableFrom(TabGridView.class), hasDescendant(cardTitleMatcher));
        ViewElement mCardViewElement =
                elements.declareView(cardSpec, elementIdOption(titleElementId));

        if (mCardIndex != null) {
            elements.declareEnterCondition(
                    new CardAtPositionCondition(
                            mCardIndex, mHostStation.getRecyclerViewElement(), mCardViewElement));
        }
    }

    protected static Matcher<View> cardTitleMatcher(String title) {
        return allOf(withText(title), withId(R.id.tab_title), withParent(CARD_MATCHER));
    }

    protected Transition.Trigger clickTitleTrigger() {
        return mCardTitleSpec::click;
    }
}
