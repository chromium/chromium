// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.context_menu;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.view.View;

import androidx.annotation.CallSuper;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;

import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator.ListItemType;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.modelutil.MVCListAdapter;

/** Station represents a opened context menu on a webpage. */
public class ContextMenuFacility extends ScrollableFacility<WebPageStation> {
    public ViewElement<View> menuListElement;

    public ContextMenuFacility() {
        menuListElement = declareView(withId(R.id.context_menu_list_view));
    }

    @CallSuper
    @Override
    protected void declareItems(ScrollableFacility<WebPageStation>.ItemsBuilder items) {
        // Context menu always has a header.
        items.declareItem(
                itemViewSpec(withId(R.id.title_and_url)),
                withMenuItemType(ListItemType.HEADER),
                null);
    }

    protected ViewSpec<View> itemViewSpec(Matcher<View> matcher) {
        return menuListElement.descendant(matcher);
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
