// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import androidx.annotation.CallSuper;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.test.R;

/** Base class for Card Facilities in the Tab Switcher. */
public abstract class TabSwitcherCardFacility extends Facility<TabSwitcherStation> {
    public static final Matcher<View> CARD_MATCHER = withId(R.id.card_view);

    protected final String mTitle;

    private ViewSpec mCardTitleSpec;

    TabSwitcherCardFacility(String title) {
        mTitle = title;
    }

    @Override
    @CallSuper
    public void declareElements(Elements.Builder elements) {
        String titleElementId = "Card title: " + mTitle;
        mCardTitleSpec = cardTitleViewSpec(mTitle);
        elements.declareView(mCardTitleSpec, ViewElement.elementIdOption(titleElementId));
    }

    protected static ViewSpec cardTitleViewSpec(String title) {
        return viewSpec(allOf(withText(title), withId(R.id.tab_title), withParent(CARD_MATCHER)));
    }

    protected Transition.Trigger clickTitleTrigger() {
        return mCardTitleSpec::click;
    }
}
