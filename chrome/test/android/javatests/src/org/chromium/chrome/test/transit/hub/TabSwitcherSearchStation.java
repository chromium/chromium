// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.OmniboxTestUtils;

/** The base station for Hub tab switcher stations. */
public class TabSwitcherSearchStation extends Station<SearchActivity> {
    public static final ViewSpec TOOLBAR = viewSpec(withId(R.id.search_location_bar));

    private final boolean mIsIncognito;
    private OmniboxTestUtils mOmniboxTestUtils;

    public TabSwitcherSearchStation(boolean isIncognito) {
        super(SearchActivity.class);
        mIsIncognito = isIncognito;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);
        elements.declareView(TOOLBAR);
    }

    public boolean isIncognito() {
        return mIsIncognito;
    }

    public SearchActivity getSearchActivity() {
        return mActivityElement.get();
    }

    public void typeInOmnibox(String query) {
        maybeInitSearchUtils();
        mOmniboxTestUtils.typeText(query, /* execute= */ false);
        mOmniboxTestUtils.waitAnimationsComplete();
    }

    public void checkSuggestionsShown(boolean shown) {
        maybeInitSearchUtils();
        mOmniboxTestUtils.checkSuggestionsShown(shown);
    }

    private void maybeInitSearchUtils() {
        if (mOmniboxTestUtils == null) {
            mOmniboxTestUtils = new OmniboxTestUtils(getSearchActivity());
        }
    }
}
