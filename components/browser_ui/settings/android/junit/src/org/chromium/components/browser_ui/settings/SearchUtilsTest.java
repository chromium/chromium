// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
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

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link SearchUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SearchUtilsTest {
    private Context mContext;
    private SearchView mSearchView;
    private View mHelpButton;
    private MenuItem mSearchItem;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(), R.style.Theme_AppCompat_Light);
        ActionMenuView menuView = new ActionMenuView(mContext);
        mSearchView = new SearchView(mContext);
        mHelpButton = new ImageView(mContext);
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

    @Test
    public void testMenuItemSearchView_closeButtonOnlyClears() {
        List<String> queries = new ArrayList<>();
        SearchUtils.initializeSearchView(
                mSearchItem,
                /* initialQuery= */ null,
                /* activity= */ null,
                /* searchViewObserver= */ null,
                queries::add);
        mSearchView.setIconified(false);
        View closeButton = mSearchView.findViewById(R.id.search_close_btn);
        assertEquals(View.GONE, closeButton.getVisibility());

        mSearchView.setQuery("foo", false);
        assertEquals(View.VISIBLE, closeButton.getVisibility());

        // The close button clears the query and hides itself.
        closeButton.performClick();
        assertEquals("", mSearchView.getQuery().toString());
        assertEquals("", queries.get(queries.size() - 1));
        assertEquals(View.GONE, closeButton.getVisibility());
    }

    /** Regression test for crbug.com/565418037. */
    @Test
    public void testStandaloneSearchView_closeButtonClearsThenCloses() {
        SearchView searchView = new SearchView(mContext);
        SearchViewProvider.Observer observer = mock(SearchViewProvider.Observer.class);
        List<String> queries = new ArrayList<>();
        SearchUtils.initializeSearchView(
                searchView, /* initialQuery= */ null, /* activity= */ null, observer, queries::add);
        searchView.setIconified(false);
        verify(observer).onUpdated(true);
        View closeButton = searchView.findViewById(R.id.search_close_btn);
        assertEquals(View.VISIBLE, closeButton.getVisibility());

        // With a query, the close button clears it and stays visible.
        searchView.setQuery("foo", false);
        queries.clear();
        closeButton.performClick();
        assertEquals("", searchView.getQuery().toString());
        assertEquals(List.of(""), queries);
        assertEquals(View.VISIBLE, closeButton.getVisibility());
        verify(observer, never()).onUpdated(false);

        // With an empty query, the close button closes the search.
        closeButton.performClick();
        verify(observer).onUpdated(false);
    }
}
