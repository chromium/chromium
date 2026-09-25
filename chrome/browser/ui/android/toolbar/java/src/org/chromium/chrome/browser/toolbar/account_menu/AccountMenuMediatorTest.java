// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.view.View.OnClickListener;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuMediator.Event;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.IdentityCardProperties;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.ItemType;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.MenuItemProperties;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.PromoCardProperties;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SigninPromoAction;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserActionableError;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;

/** Unit tests for {@link AccountMenuMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS,
    SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
})
public class AccountMenuMediatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private Activity mActivity;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private @Nullable Profile mProfile;
    @Mock private TabCreator mIncognitoTabCreator;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private MultiInstanceOrchestrator mOrchestrator;
    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private Runnable mDismissCallback;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private SigninManager mSigninManager;
    @Mock private SyncService mSyncService;
    @Mock private BottomSheetSigninAndHistorySyncCoordinator mSigninCoordinator;
    @Mock private SigninAndHistorySyncActivityLauncher mSigninLauncher;
    @Mock private SigninMetricsUtils.Natives mSigninMetricsUtilsNativeMock;

    private Context mContext;
    private ModelList mModelList;
    private AccountMenuMediator mMediator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        doReturn(mSigninManager).when(mIdentityServicesProvider).getSigninManager(mProfile);
        doReturn(mAccountManagerTestRule.getIdentityManager())
                .when(mIdentityServicesProvider)
                .getIdentityManager(mProfile);
        doReturn(UserActionableError.NONE).when(mSyncService).getUserActionableError();
        doReturn(true).when(mSigninManager).isSigninAllowed();
        MultiInstanceOrchestratorFactory.setInstanceForTesting(mOrchestrator);
        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelector);
        doReturn(mTabCreatorManager).when(mTabModelSelector).getTabCreatorManager();
        doReturn(mIncognitoTabCreator).when(mTabCreatorManager).getTabCreator(true);

        IncognitoUtils.setEnabledForTesting(true);

        SigninMetricsUtilsJni.setInstanceForTesting(mSigninMetricsUtilsNativeMock);

        mModelList = new ModelList();
        mMediator =
                new AccountMenuMediator(
                        mContext,
                        mProfile,
                        mWindowAndroid,
                        mModelList,
                        mSigninCoordinator,
                        mSigninLauncher,
                        mDismissCallback);
    }

    @After
    public void tearDown() {
        mMediator.destroy();
        SettingsNavigationFactory.setInstanceForTesting(null);
        IdentityServicesProvider.setInstanceForTests(null);
        SyncServiceFactory.setInstanceForTesting(null);
    }

    @Test
    @DisableFeatures(SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT)
    public void testSignedOut_showsPromoCardAndSigninsOnClick() {
        assertEquals(4, mModelList.size());
        ListItem item = mModelList.get(0);
        assertEquals(ItemType.PROMO_CARD, item.type);

        OnClickListener onSigninClick =
                item.model.get(PromoCardProperties.ON_SIGNIN_CLICK_LISTENER);
        assertNotNull(onSigninClick);

        onSigninClick.onClick(null);

        verify(mDismissCallback).run();
        verify(mSigninLauncher)
                .createBottomSheetSigninIntentOrShowError(
                        eq(mContext),
                        eq(mProfile),
                        any(),
                        eq(SigninAccessPoint.ACCOUNT_MENU_SIGNED_OUT_STATE));
    }

    @Test
    @EnableFeatures(SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT)
    public void testSignedOut_activitylessSigninOnClick() {
        assertEquals(4, mModelList.size());
        ListItem item = mModelList.get(0);
        assertEquals(ItemType.PROMO_CARD, item.type);

        OnClickListener onSigninClick =
                item.model.get(PromoCardProperties.ON_SIGNIN_CLICK_LISTENER);
        assertNotNull(onSigninClick);

        onSigninClick.onClick(null);

        verify(mDismissCallback).run();
        verify(mSigninCoordinator).startSigninFlow(any());
    }

    @Test
    public void testSigninNotAllowed_omitsPromoCard() {
        doReturn(false).when(mSigninManager).isSigninAllowed();
        mMediator.updateMenuItems(/* recordShownMetrics= */ false);

        assertEquals(3, mModelList.size());
        assertEquals(ItemType.MENU_ITEM, mModelList.get(0).type);
        assertEquals(
                R.string.menu_passwords_and_autofill,
                mModelList.get(0).model.get(MenuItemProperties.TITLE_ID));
    }

    @Test
    public void testAutofillItemClick_dismissesAndOpensAutofillSettings() {
        assertEquals(4, mModelList.size());
        ListItem item = mModelList.get(1);
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
    public void testAccountSettingsItemClick_dismissesAndOpensAccountSettings() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);
        mMediator.updateMenuItems(/* recordShownMetrics= */ false);

        assertEquals(6, mModelList.size());
        ListItem item = mModelList.get(3);
        assertEquals(ItemType.MENU_ITEM, item.type);

        PropertyModel model = item.model;
        assertEquals(
                R.string.profile_menu_account_settings_button,
                model.get(MenuItemProperties.TITLE_ID));
        assertEquals(R.drawable.settings_cog, model.get(MenuItemProperties.START_ICON_ID));

        OnClickListener clickListener = model.get(MenuItemProperties.CLICK_LISTENER);
        assertNotNull(clickListener);

        clickListener.onClick(null);

        verify(mDismissCallback).run();
        verify(mSettingsNavigation).startSettings(mContext, SettingsFragment.MAIN);
    }

    @Test
    public void testOpenIncognitoItemClick_dismissesAndOpensNewIncognitoWindow() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        mMediator.updateMenuItems(/* recordShownMetrics= */ false);

        doReturn(new WeakReference<>(mActivity)).when(mWindowAndroid).getActivity();

        assertEquals(4, mModelList.size());
        assertEquals(ItemType.DIVIDER, mModelList.get(2).type);
        ListItem item = mModelList.get(3);
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
    public void testOpenIncognitoItemClick_dismissesAndOpensNewIncognitoTab() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(false);
        mMediator.updateMenuItems(/* recordShownMetrics= */ false);

        assertEquals(4, mModelList.size());
        ListItem item = mModelList.get(3);
        assertEquals(ItemType.MENU_ITEM, item.type);
        assertEquals(R.string.menu_new_incognito_tab, item.model.get(MenuItemProperties.TITLE_ID));

        OnClickListener clickListener = item.model.get(MenuItemProperties.CLICK_LISTENER);
        assertNotNull(clickListener);
        clickListener.onClick(null);

        verify(mDismissCallback).run();
        verify(mIncognitoTabCreator).launchNtp(TabLaunchType.FROM_CHROME_UI);
    }

    @Test
    public void testOpenIncognitoDisabled_omitsIncognitoItemAndDivider() {
        IncognitoUtils.setEnabledForTesting(false);
        mMediator.updateMenuItems(/* recordShownMetrics= */ false);

        assertEquals(2, mModelList.size());
        assertEquals(ItemType.PROMO_CARD, mModelList.get(0).type);
        assertEquals(ItemType.MENU_ITEM, mModelList.get(1).type);
        assertEquals(
                R.string.menu_passwords_and_autofill,
                mModelList.get(1).model.get(MenuItemProperties.TITLE_ID));
    }

    @Test
    public void testSignedIn_showsIdentityCard() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);

        mMediator.updateMenuItems(/* recordShownMetrics= */ false);

        assertEquals(6, mModelList.size());
        ListItem item = mModelList.get(0);
        assertEquals(ItemType.IDENTITY_CARD, item.type);
        DisplayableProfileData profileData = item.model.get(IdentityCardProperties.PROFILE_DATA);
        assertNotNull(profileData);
        assertEquals(TestAccounts.ACCOUNT1.getFullName(), profileData.getFullName());
        assertEquals(TestAccounts.ACCOUNT1.getEmail(), profileData.getAccountEmail());
    }

    @Test
    public void testProfileDataUpdated_updatesIdentityCard() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);

        mMediator.updateMenuItems(/* recordShownMetrics= */ false);

        ListItem item = mModelList.get(0);
        assertEquals(ItemType.IDENTITY_CARD, item.type);
        DisplayableProfileData initialProfileData =
                item.model.get(IdentityCardProperties.PROFILE_DATA);
        assertNotNull(initialProfileData);
        assertEquals(TestAccounts.ACCOUNT1.getFullName(), initialProfileData.getFullName());
        assertEquals(TestAccounts.ACCOUNT1.getEmail(), initialProfileData.getAccountEmail());

        // When profile data updates for another account, identity card is not updated.
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        mAccountManagerTestRule.updateAccount(TestAccounts.ACCOUNT2);
        assertEquals(initialProfileData, item.model.get(IdentityCardProperties.PROFILE_DATA));

        // When profile data updates for the primary account, identity card is updated.
        AccountInfo updatedAccount =
                new AccountInfo.Builder(TestAccounts.ACCOUNT1).fullName("Updated Name").build();
        mAccountManagerTestRule.updateAccount(updatedAccount);

        DisplayableProfileData updatedProfileData =
                item.model.get(IdentityCardProperties.PROFILE_DATA);
        assertNotNull(updatedProfileData);
        assertEquals("Updated Name", updatedProfileData.getFullName());
        assertEquals(TestAccounts.ACCOUNT1.getEmail(), updatedProfileData.getAccountEmail());
    }

    @Test
    public void testObserverRegistrationAndTeardown() {
        verify(mSigninManager).addSignInStateObserver(mMediator);

        mMediator.destroy();
        verify(mSigninManager).removeSignInStateObserver(mMediator);
    }

    @Test
    public void testSignInStateObservers_updateMenuItems() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);
        mMediator.onSignedIn();
        // Identity card, autofill, account settings, manage Google account, divider and incognito
        // items.
        assertEquals(6, mModelList.size());
        assertEquals(ItemType.IDENTITY_CARD, mModelList.get(0).type);

        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(null);
        mMediator.onSignedOut();
        assertEquals(4, mModelList.size());
        assertEquals(ItemType.PROMO_CARD, mModelList.get(0).type);

        doReturn(false).when(mSigninManager).isSigninAllowed();
        mMediator.onSignInAllowedChanged();
        assertEquals(3, mModelList.size());
        assertEquals(ItemType.MENU_ITEM, mModelList.get(0).type);
    }

    @Test
    public void testManageGoogleAccountItemClick_opensMyAccount() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);

        mMediator.updateMenuItems(/* recordShownMetrics= */ false);

        assertEquals(6, mModelList.size());
        ListItem item = mModelList.get(2);
        assertEquals(ItemType.MENU_ITEM, item.type);
        assertNotNull(item);
        assertEquals(
                R.string.manage_your_google_account, item.model.get(MenuItemProperties.TITLE_ID));
        OnClickListener clickListener = item.model.get(MenuItemProperties.CLICK_LISTENER);
        assertNotNull(clickListener);

        clickListener.onClick(null);

        verify(mDismissCallback).run();
        verify(mSigninLauncher).openManageGoogleAccount(eq(mContext));
    }

    @Test
    public void testSignedOut_doesNotDisplayManageGoogleAccountItem() {
        // The FakeIdentityManager from `mAccountManagerTestRule` has no primary account by default.
        mMediator.updateMenuItems(/* recordShownMetrics= */ false);

        for (ListItem item : mModelList) {
            if (item.type == ItemType.MENU_ITEM) {
                assertNotEquals(
                        R.string.manage_your_google_account,
                        item.model.get(MenuItemProperties.TITLE_ID));
            }
        }
    }

    @Test
    public void testIncognitoProfile_doesNotDisplayManageGoogleAccountItem() {
        doReturn(true).when(mProfile).isOffTheRecord();

        mMediator.updateMenuItems(/* recordShownMetrics= */ false);

        for (ListItem item : mModelList) {
            if (item.type == ItemType.MENU_ITEM) {
                assertNotEquals(
                        R.string.manage_your_google_account,
                        item.model.get(MenuItemProperties.TITLE_ID));
            }
        }
    }

    @Test
    public void testRecordMenuShown_signedOut() {
        HistogramWatcher watcher = expectEvent(Event.SHOWN_SIGNED_OUT);

        mMediator.updateMenuItems(/* recordShownMetrics= */ true);

        watcher.assertExpected();
        verify(mSigninMetricsUtilsNativeMock)
                .logSigninOffered(
                        SigninPromoAction.NO_SIGNIN_PROMO,
                        SigninAccessPoint.ACCOUNT_MENU_SIGNED_OUT_STATE);
    }

    @Test
    public void testRecordMenuShown_signinNotAllowed() {
        doReturn(false).when(mSigninManager).isSigninAllowed();
        HistogramWatcher watcher = expectEvent(Event.SHOWN_SIGNED_OUT_SIGNIN_DISABLED);

        mMediator.updateMenuItems(/* recordShownMetrics= */ true);

        watcher.assertExpected();
        // Sign-in was not offered, since no promo card is shown.
        verify(mSigninMetricsUtilsNativeMock, never()).logSigninOffered(anyInt(), anyInt());
    }

    @Test
    public void testRecordMenuShown_signedIn() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);
        HistogramWatcher watcher = expectEvent(Event.SHOWN_SIGNED_IN);

        mMediator.updateMenuItems(/* recordShownMetrics= */ true);

        watcher.assertExpected();
        verify(mSigninMetricsUtilsNativeMock, never()).logSigninOffered(anyInt(), anyInt());
    }

    @Test
    public void testRecordMenuShown_signedInWithError() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);
        doReturn(UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_EVERYTHING)
                .when(mSyncService)
                .getUserActionableError();
        HistogramWatcher watcher = expectEvent(Event.SHOWN_SIGNED_IN_WITH_ERROR);

        mMediator.updateMenuItems(/* recordShownMetrics= */ true);

        watcher.assertExpected();
    }

    @Test
    public void testRecordEvent_signinPromo() {
        HistogramWatcher watcher = expectEvent(Event.SIGNIN_PROMO_CLICKED);

        ListItem item = mModelList.get(0);
        assertEquals(ItemType.PROMO_CARD, item.type);
        OnClickListener onSigninClick =
                item.model.get(PromoCardProperties.ON_SIGNIN_CLICK_LISTENER);
        assertNotNull(onSigninClick);
        onSigninClick.onClick(null);

        watcher.assertExpected();
    }

    @Test
    public void testRecordEvent_passwordsAndAutofill() {
        HistogramWatcher watcher = expectEvent(Event.PASSWORDS_AND_AUTOFILL_CLICKED);

        clickMenuItem(/* index= */ 1);

        watcher.assertExpected();
    }

    @Test
    public void testRecordEvent_newIncognitoWindow() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        mMediator.updateMenuItems(/* recordShownMetrics= */ false);
        doReturn(new WeakReference<>(mActivity)).when(mWindowAndroid).getActivity();
        HistogramWatcher watcher = expectEvent(Event.NEW_INCOGNITO_WINDOW_CLICKED);

        clickMenuItem(/* index= */ 3);

        watcher.assertExpected();
    }

    @Test
    public void testRecordEvent_newIncognitoTab() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(false);
        mMediator.updateMenuItems(/* recordShownMetrics= */ false);
        HistogramWatcher watcher = expectEvent(Event.NEW_INCOGNITO_TAB_CLICKED);

        clickMenuItem(/* index= */ 3);

        watcher.assertExpected();
    }

    @Test
    public void testRecordEvent_manageGoogleAccount() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);
        mMediator.updateMenuItems(/* recordShownMetrics= */ false);
        HistogramWatcher watcher = expectEvent(Event.MANAGE_GOOGLE_ACCOUNT_CLICKED);

        clickMenuItem(/* index= */ 2);

        watcher.assertExpected();
    }

    @Test
    public void testRecordEvent_accountSettings() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);
        mMediator.updateMenuItems(/* recordShownMetrics= */ false);
        HistogramWatcher watcher = expectEvent(Event.ACCOUNT_SETTINGS_CLICKED);

        clickMenuItem(/* index= */ 3);

        watcher.assertExpected();
    }

    private void clickMenuItem(int index) {
        ListItem item = mModelList.get(index);
        assertEquals(ItemType.MENU_ITEM, item.type);
        OnClickListener clickListener = item.model.get(MenuItemProperties.CLICK_LISTENER);
        assertNotNull(clickListener);
        clickListener.onClick(null);
    }

    /** Expects the given event to be recorded once. */
    private static HistogramWatcher expectEvent(@Event int event) {
        return HistogramWatcher.newSingleRecordWatcher("Signin.AccountMenu.Event", event);
    }
}
