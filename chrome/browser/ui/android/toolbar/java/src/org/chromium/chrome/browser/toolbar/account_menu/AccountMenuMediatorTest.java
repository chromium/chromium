// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.app.Activity;
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
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.ItemType;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.MenuItemProperties;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;

/** Unit tests for {@link AccountMenuMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountMenuMediatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Profile mProfile;
    @Mock private TabCreator mIncognitoTabCreator;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private MultiInstanceOrchestrator mOrchestrator;
    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private Runnable mDismissCallback;

    private Context mContext;
    private ModelList mModelList;
    private AccountMenuMediator mMediator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(mOrchestrator);
        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelector);
        doReturn(mTabCreatorManager).when(mTabModelSelector).getTabCreatorManager();
        doReturn(mIncognitoTabCreator).when(mTabCreatorManager).getTabCreator(true);

        IncognitoUtils.setEnabledForTesting(true);

        mModelList = new ModelList();
        mMediator =
                new AccountMenuMediator(
                        mContext, mModelList, mWindowAndroid, () -> mProfile, mDismissCallback);
    }

    @After
    public void tearDown() {
        SettingsNavigationFactory.setInstanceForTesting(null);
    }

    @Test
    @SmallTest
    public void testAutofillItemClick_dismissesAndOpensAutofillSettings() {
        assertEquals(3, mModelList.size());
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

    @Test
    @SmallTest
    public void testOpenIncognitoItemClick_dismissesAndOpensNewIncognitoWindow() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        mMediator.updateMenuItems();

        doReturn(new WeakReference<>(mActivity)).when(mWindowAndroid).getActivity();

        assertEquals(3, mModelList.size());
        assertEquals(ItemType.DIVIDER, mModelList.get(1).type);
        ListItem item = mModelList.get(2);
        assertEquals(ItemType.MENU_ITEM, item.type);
        assertEquals(
                R.string.menu_new_incognito_window, item.model.get(MenuItemProperties.TITLE_ID));
        assertEquals(
                R.drawable.ic_incognito_24dp, item.model.get(MenuItemProperties.START_ICON_ID));

        OnClickListener clickListener = item.model.get(MenuItemProperties.CLICK_LISTENER);
        assertNotNull(clickListener);
        clickListener.onClick(null);

        verify(mDismissCallback).run();
        verify(mOrchestrator)
                .createNewWindow(
                        eq(mActivity),
                        eq(true),
                        isNull(),
                        isNull(),
                        eq(NewWindowAppSource.ACCOUNT_MENU));
    }

    @Test
    @SmallTest
    public void testOpenIncognitoItemClick_dismissesAndOpensNewIncognitoTab() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(false);
        mMediator.updateMenuItems();

        assertEquals(3, mModelList.size());
        ListItem item = mModelList.get(2);
        assertEquals(ItemType.MENU_ITEM, item.type);
        assertEquals(R.string.menu_new_incognito_tab, item.model.get(MenuItemProperties.TITLE_ID));

        OnClickListener clickListener = item.model.get(MenuItemProperties.CLICK_LISTENER);
        assertNotNull(clickListener);
        clickListener.onClick(null);

        verify(mDismissCallback).run();
        verify(mIncognitoTabCreator).launchNtp(TabLaunchType.FROM_CHROME_UI);
    }

    @Test
    @SmallTest
    public void testOpenIncognitoDisabled_omitsIncognitoItemAndDivider() {
        IncognitoUtils.setEnabledForTesting(false);
        mMediator.updateMenuItems();

        assertEquals(1, mModelList.size());
        assertEquals(ItemType.MENU_ITEM, mModelList.get(0).type);
        assertEquals(
                R.string.menu_passwords_and_autofill,
                mModelList.get(0).model.get(MenuItemProperties.TITLE_ID));
    }
}
