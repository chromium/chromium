// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.MenuItem;
import android.view.View;
import android.widget.ImageView;

import androidx.appcompat.view.ContextThemeWrapper;
import androidx.appcompat.widget.ActionMenuView;
import androidx.appcompat.widget.SearchView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link SearchUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SearchUtilsTest {
    private SearchView mSearchView;
    private View mHelpButton;
    private MenuItem mSearchItem;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(), R.style.Theme_AppCompat_Light);
        ActionMenuView menuView = new ActionMenuView(context);
        mSearchView = new SearchView(context);
        mHelpButton = new ImageView(context);
        menuView.addView(mSearchView);
        menuView.addView(mHelpButton);

        mSearchItem = mock(MenuItem.class);
        when(mSearchItem.getActionView()).thenReturn(mSearchView);
    }

    /**
     * Regression test for crbug.com/565633710: The open search view fills the toolbar, so other
     * action items like the help icon must be hidden to avoid clipping them.
     */
    @Test
    public void testHidesSiblingActionItemsWhileSearching() {
        SearchUtils.initializeSearchView(
                mSearchItem,
                /* initialQuery= */ null,
                /* activity= */ null,
                /* searchViewObserver= */ null,
                query -> {});
        assertEquals(View.VISIBLE, mHelpButton.getVisibility());

        // Open the search view.
        mSearchView.setIconified(false);
        assertEquals(View.GONE, mHelpButton.getVisibility());
        assertEquals(View.VISIBLE, mSearchView.getVisibility());

        // Typing keeps the other action items hidden.
        mSearchView.setQuery("foo", false);
        assertEquals(View.GONE, mHelpButton.getVisibility());

        // Closing the search view shows them again.
        SearchUtils.clearSearch(mSearchItem, /* activity= */ null);
        assertEquals(View.VISIBLE, mHelpButton.getVisibility());
    }

    @Test
    public void testHidesSiblingActionItemsWhenRestoringQuery() {
        SearchUtils.initializeSearchView(
                mSearchItem,
                /* initialQuery= */ "foo",
                /* activity= */ null,
                /* searchViewObserver= */ null,
                query -> {});
        assertEquals(View.GONE, mHelpButton.getVisibility());
    }
}
