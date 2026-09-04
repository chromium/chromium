// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
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
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link UrlBarContextMenuHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UrlBarContextMenuHelperUnitTest {
    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private UrlBar mUrlBar;
    @Mock private ContextMenu mContextMenu;
    @Mock private MenuItem mCopyMenuItem;
    @Mock private UrlBarContextMenuHelper.Delegate mDelegate;

    private UrlBarContextMenuHelper mHelper;
    private TestActivity mActivity;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        lenient().doReturn(mActivity).when(mUrlBar).getContext();
        lenient().doReturn(mActivity.getResources()).when(mUrlBar).getResources();
        lenient().doReturn(100).when(mUrlBar).getWidth();
        lenient().doReturn(mUrlBar).when(mUrlBar).getRootView();
        lenient().doReturn(50).when(mUrlBar).getHeight();

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
        mHelper.onMenuItemClicked(R.id.url_bar_manage_search_engines);
        verify(mDelegate).onTextContextMenuItem(R.id.url_bar_manage_search_engines);
    }

    @Test
    public void testMenuItemClick_pasteAndGo() {
        mHelper.onMenuItemClicked(R.id.url_bar_paste_and_go);
        verify(mDelegate).onTextContextMenuItem(R.id.url_bar_paste_and_go);
    }

    @Test
    public void testShowListMenu_filtersOutNonAllowedItems() {
        setupMockContextMenu(android.R.id.button1, null);

        mHelper.showListMenu(mContextMenu);
        assertEquals(0, mHelper.getModelListForTesting().size());
    }

    @Test
    public void testShowListMenu_ordersItemsAndAddsDividers() {
        MenuItem undoItem = org.mockito.Mockito.mock(MenuItem.class);
        doReturn(android.R.id.undo).when(undoItem).getItemId();
        doReturn("Undo").when(undoItem).getTitle();
        doReturn(true).when(undoItem).isVisible();
        doReturn(true).when(undoItem).isEnabled();

        MenuItem copyItem = org.mockito.Mockito.mock(MenuItem.class);
        doReturn(android.R.id.copy).when(copyItem).getItemId();
        doReturn("Copy").when(copyItem).getTitle();
        doReturn(true).when(copyItem).isVisible();
        doReturn(true).when(copyItem).isEnabled();

        MenuItem selectAllItem = org.mockito.Mockito.mock(MenuItem.class);
        doReturn(android.R.id.selectAll).when(selectAllItem).getItemId();
        doReturn("Select all").when(selectAllItem).getTitle();
        doReturn(true).when(selectAllItem).isVisible();
        doReturn(true).when(selectAllItem).isEnabled();

        doReturn(3).when(mContextMenu).size();
        doReturn(true).when(mContextMenu).hasVisibleItems();
        doReturn(selectAllItem).when(mContextMenu).getItem(0);
        doReturn(undoItem).when(mContextMenu).getItem(1);
        doReturn(copyItem).when(mContextMenu).getItem(2);

        mHelper.showListMenu(mContextMenu);

        assertEquals(5, mHelper.getModelListForTesting().size());
        assertEquals(
                android.R.id.undo,
                mHelper.getModelListForTesting()
                        .get(0)
                        .model
                        .get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                android.R.id.copy,
                mHelper.getModelListForTesting()
                        .get(2)
                        .model
                        .get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                android.R.id.selectAll,
                mHelper.getModelListForTesting()
                        .get(4)
                        .model
                        .get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    private void setupMockContextMenu() {
        setupMockContextMenu(android.R.id.copy, "Copy");
    }

    private void setupMockContextMenu(int itemId, String title) {
        doReturn(1).when(mContextMenu).size();
        doReturn(true).when(mContextMenu).hasVisibleItems();
        doReturn(itemId).when(mCopyMenuItem).getItemId();
        if (title != null) {
            doReturn(title).when(mCopyMenuItem).getTitle();
        }
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
        setupMockContextMenu(R.id.url_bar_always_show_ai_mode, "Always show AI mode");
        doReturn(true).when(mCopyMenuItem).isCheckable();
        doReturn(isChecked).when(mCopyMenuItem).isChecked();

        mHelper.showListMenu(mContextMenu);
        assertEquals(1, mHelper.getModelListForTesting().size());

        ListItem item = mHelper.getModelListForTesting().get(0);
        PropertyModel model = item.model;
        assertEquals(
                R.id.url_bar_always_show_ai_mode, model.get(ListMenuItemProperties.MENU_ITEM_ID));
        int expectedIcon = isChecked ? R.drawable.ic_done_blue : 0;
        assertEquals(expectedIcon, model.get(ListMenuItemProperties.END_ICON_ID));
    }
}
