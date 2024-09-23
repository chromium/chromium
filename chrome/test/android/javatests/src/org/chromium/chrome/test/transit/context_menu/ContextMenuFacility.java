// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.context_menu;

import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.IdRes;
import androidx.annotation.StringRes;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator.ListItemType;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.modelutil.MVCListAdapter;

/** Station represents a opened context menu on a webpage. */
public class ContextMenuFacility extends ScrollableFacility<WebPageStation> {

    public static final Matcher<View> CONTEXT_MENU_LIST_MATCHER =
            withId(R.id.context_menu_list_view);
    public static final ViewSpec MENU_LIST = viewSpec(CONTEXT_MENU_LIST_MATCHER);

    @CallSuper
    @Override
    protected void declareItems(ScrollableFacility<WebPageStation>.ItemsBuilder items) {
        // Context menu always has a header.
        items.declareItem(
                itemViewMatcher(R.id.title_and_url), withMenuItemType(ListItemType.HEADER), null);
    }

    @CallSuper
    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(MENU_LIST);
        super.declareElements(elements);
    }

    @Override
    protected int getMinimumOnScreenItemCount() {
        // Expect at least the first two menu items, it's enough to establish the transition is
        // done.
        return 2;
    }

    protected static Matcher<View> itemViewMatcher(@IdRes int id) {
        return allOf(withId(id), isDescendantOfA(CONTEXT_MENU_LIST_MATCHER));
    }

    protected static Matcher<View> itemViewMatcherWithText(@StringRes int stringRes) {
        return allOf(withText(stringRes), isDescendantOfA(CONTEXT_MENU_LIST_MATCHER));
    }

    protected static Matcher<MVCListAdapter.ListItem> withMenuItemType(@ListItemType int type) {
        return new TypeSafeMatcher<>() {
            @Override
            public void describeTo(Description description) {
                description.appendText("with list item type ");
                description.appendText(String.valueOf(type));
            }

            @Override
            protected boolean matchesSafely(MVCListAdapter.ListItem listItem) {
                return listItem.type == type;
            }
        };
    }
}
