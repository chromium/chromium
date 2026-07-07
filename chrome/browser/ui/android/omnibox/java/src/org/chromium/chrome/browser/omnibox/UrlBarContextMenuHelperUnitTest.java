// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.view.ContextMenu;
import android.view.MenuItem;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link UrlBarContextMenuHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(OmniboxFeatureList.OMNIBOX_LIST_MENU_CONTEXT_MENU)
public class UrlBarContextMenuHelperUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UrlBar mUrlBar;
    @Mock private ContextMenu mContextMenu;
    @Mock private MenuItem mCopyMenuItem;
    @Mock private UrlBarContextMenuHelper.Delegate mDelegate;

    private UrlBarContextMenuHelper mHelper;
    private TestActivity mActivity;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        doReturn(mActivity).when(mUrlBar).getContext();
        doReturn(mActivity.getResources()).when(mUrlBar).getResources();
        doReturn(100).when(mUrlBar).getWidth();
        doReturn(mUrlBar).when(mUrlBar).getRootView();
        doReturn(50).when(mUrlBar).getHeight();

        mHelper = new UrlBarContextMenuHelper(mUrlBar, mDelegate);
    }

    @Test
    public void testOnCreateContextMenu_addsMenuItems() {
        setupMockContextMenu();

        mHelper.onCreateContextMenu(mContextMenu, mUrlBar, null);
        assertTrue(mHelper.getModelListForTesting().size() > 0);
    }

    @Test
    public void testDestroy_dismissesListMenuHost() {
        setupMockContextMenu();

        mHelper.onCreateContextMenu(mContextMenu, mUrlBar, null);
        assertTrue(mHelper.getModelListForTesting().size() > 0);

        mHelper.destroy();
    }

    @Test
    public void testMenuItemClick_callsOnTextContextMenuItem() {
        mHelper.onMenuItemClicked(android.R.id.copy);
        verify(mDelegate).onTextContextMenuItem(android.R.id.copy);
    }

    @Test
    public void testMenuItemClick_manageSearchEngines() {
        int[] called = {0};
        Runnable callback = () -> called[0]++;
        doReturn(callback).when(mDelegate).getManageSearchEnginesCallback();
        mHelper.onMenuItemClicked(R.id.url_bar_manage_search_engines);
        assertEquals(1, called[0]);
    }

    @Test
    public void testOnCreateContextMenu_filtersOutNonAllowedItems() {
        setupMockContextMenu();
        doReturn(android.R.id.button1).when(mCopyMenuItem).getItemId();

        mHelper.onCreateContextMenu(mContextMenu, mUrlBar, null);
        assertEquals(0, mHelper.getModelListForTesting().size());
        verify(mContextMenu).clear();
    }

    @Test
    public void testOnCreateContextMenu_allowsShareText() {
        setupMockContextMenu();
        doReturn(android.R.id.shareText).when(mCopyMenuItem).getItemId();

        mHelper.onCreateContextMenu(mContextMenu, mUrlBar, null);
        assertTrue(mHelper.getModelListForTesting().size() > 0);
    }

    private void setupMockContextMenu() {
        doReturn(1).when(mContextMenu).size();
        doReturn(true).when(mContextMenu).hasVisibleItems();
        doReturn(android.R.id.copy).when(mCopyMenuItem).getItemId();
        doReturn("Copy").when(mCopyMenuItem).getTitle();
        doReturn(true).when(mCopyMenuItem).isVisible();
        doReturn(mCopyMenuItem).when(mContextMenu).getItem(0);
    }
}
