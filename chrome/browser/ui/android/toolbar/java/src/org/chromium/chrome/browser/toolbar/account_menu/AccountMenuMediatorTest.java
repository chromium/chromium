// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.View.OnClickListener;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.ItemType;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.MenuItemProperties;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link AccountMenuMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountMenuMediatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private Runnable mDismissCallback;

    private Context mContext;
    private ModelList mModelList;
    private AccountMenuMediator mMediator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);

        mModelList = new ModelList();
        mMediator = new AccountMenuMediator(mContext, mModelList, mDismissCallback);
    }

    @After
    public void tearDown() {
        SettingsNavigationFactory.setInstanceForTesting(null);
    }

    @Test
    @SmallTest
    public void testAutofillItemClick_dismissesAndOpensAutofillSettings() {
        assertEquals(1, mModelList.size());
        ListItem item = mModelList.get(0);
        assertEquals(ItemType.MENU_ITEM, item.type);

        PropertyModel model = item.model;
        assertEquals(R.string.menu_passwords_and_autofill, model.get(MenuItemProperties.TITLE_ID));
        assertEquals(
                R.drawable.ic_password_manager_24dp, model.get(MenuItemProperties.START_ICON_ID));

        OnClickListener clickListener = model.get(MenuItemProperties.CLICK_LISTENER);
        assertNotNull(clickListener);

        clickListener.onClick(null);

        verify(mDismissCallback).run();
        verify(mSettingsNavigation)
                .startSettings(mContext, SettingsFragment.AUTOFILL_AND_PASSWORDS);
    }
}
