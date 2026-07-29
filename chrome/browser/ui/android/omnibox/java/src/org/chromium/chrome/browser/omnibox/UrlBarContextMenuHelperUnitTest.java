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
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link UrlBarContextMenuHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
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
    public void testShowListMenu_addsMenuItems() {
        setupMockContextMenu();

        mHelper.showListMenu(mContextMenu);
        assertTrue(mHelper.getModelListForTesting().size() > 0);
    }

    @Test
    public void testDestroy_dismissesListMenuHost() {
        setupMockContextMenu();

        mHelper.showListMenu(mContextMenu);
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
    public void testShowListMenu_filtersOutNonAllowedItems() {
        setupMockContextMenu();
        doReturn(android.R.id.button1).when(mCopyMenuItem).getItemId();

        mHelper.showListMenu(mContextMenu);
        assertEquals(0, mHelper.getModelListForTesting().size());
    }

    @Test
    public void testShowListMenu_allowsShareText() {
        setupMockContextMenu();
        doReturn(android.R.id.shareText).when(mCopyMenuItem).getItemId();

        mHelper.showListMenu(mContextMenu);
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

    @Test
    public void testShowListMenu_allowsAlwaysShowAiMode_checked() {
        verifyAlwaysShowAiMode(true);
    }

    @Test
    public void testShowListMenu_allowsAlwaysShowAiMode_unchecked() {
        verifyAlwaysShowAiMode(false);
    }

    private void verifyAlwaysShowAiMode(boolean isChecked) {
        setupMockContextMenu();
        doReturn(R.id.url_bar_always_show_ai_mode).when(mCopyMenuItem).getItemId();
        doReturn("Always show AI mode").when(mCopyMenuItem).getTitle();
        doReturn(true).when(mCopyMenuItem).isCheckable();
        doReturn(isChecked).when(mCopyMenuItem).isChecked();

        mHelper.showListMenu(mContextMenu);
        assertEquals(1, mHelper.getModelListForTesting().size());

        ListItem item = mHelper.getModelListForTesting().get(0);
        PropertyModel model = item.model;
        assertEquals(
                R.id.url_bar_always_show_ai_mode, model.get(ListMenuItemProperties.MENU_ITEM_ID));
        int expectedIcon = isChecked ? R.drawable.ic_done_blue : 0;
        assertEquals(expectedIcon, model.get(ListMenuItemProperties.START_ICON_ID));
    }
}
