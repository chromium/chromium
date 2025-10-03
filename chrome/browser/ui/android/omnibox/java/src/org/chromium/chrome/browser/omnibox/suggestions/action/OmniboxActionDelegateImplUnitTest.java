// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.robolectric.Shadows.shadowOf;

import android.app.Application;
import android.content.Context;
import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ActivityState;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Consumer;

/** Tests for {@link OmniboxActionDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxActionDelegateImplUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock Consumer<String> mMockOpenUrl;
    private @Mock Runnable mMockOpenIncognitoPage;
    private @Mock Runnable mMockOpenPasswordSettings;
    private @Mock SettingsNavigation mMockSettingsNavigation;
    private @Mock Tab mTab;
    private @Mock Runnable mMockOpenQuickDeleteDialog;
    private final AtomicReference<Tab> mTabReference = new AtomicReference<>();
    private Context mContext;
    private OmniboxActionDelegateImpl mDelegate;

    private @Mock TabWindowManager mTabManager;
    private @Mock Callback<Tab> mBringTabToFrontCallback;
    private @Mock WindowAndroid mMockWindowAndroid;
    private @Mock TabModel mTabModel;
    private ObservableSupplierImpl<TabWindowManager> mTabManagerSupplier;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mTabReference.set(mTab);
        mTabManagerSupplier = new ObservableSupplierImpl<>();
        mDelegate =
                new OmniboxActionDelegateImpl(
                        mContext,
                        () -> mTabReference.get(),
                        mMockOpenUrl,
                        mMockOpenIncognitoPage,
                        mMockOpenPasswordSettings,
                        mMockOpenQuickDeleteDialog,
                        mTabManagerSupplier,
                        mBringTabToFrontCallback);
        SettingsNavigationFactory.setInstanceForTesting(mMockSettingsNavigation);
        doAnswer(inv -> Collections.emptyList().iterator()).when(mTabModel).iterator();
    }

    @After
    public void cleanUp() {
        verifyNoMoreInteractions(mMockOpenIncognitoPage);
        verifyNoMoreInteractions(mMockOpenPasswordSettings);
        verifyNoMoreInteractions(mMockOpenUrl);
        verifyNoMoreInteractions(mMockOpenQuickDeleteDialog);
    }

    @Test
    public void openIncognitoTab() {
        mDelegate.openIncognitoTab();
        verify(mMockOpenIncognitoPage, times(1)).run();
    }

    @Test
    public void openPasswordManager() {
        mDelegate.openPasswordManager();
        verify(mMockOpenPasswordSettings, times(1)).run();
    }

    @Test
    public void openSettingsPage() {
        mDelegate.openSettingsPage(SettingsFragment.ACCESSIBILITY);
        verify(mMockSettingsNavigation, times(1))
                .startSettings(mContext, SettingsFragment.ACCESSIBILITY);
    }

    @Test
    public void isIncognito() {
        doReturn(true).when(mTab).isIncognito();
        assertTrue(mDelegate.isIncognito());

        doReturn(false).when(mTab).isIncognito();
        assertFalse(mDelegate.isIncognito());

        mTabReference.set(null);
        assertFalse(mDelegate.isIncognito());
    }

    @Test
    public void loadPageInCurrentTab_useCurrentInteractableTab() {
        doReturn(true).when(mTab).isUserInteractable();
        mDelegate.loadPageInCurrentTab("url");

        verify(mTab, times(1)).isUserInteractable();
        var loadParamsCaptor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mTab, times(1)).loadUrl(loadParamsCaptor.capture());
        assertEquals("url", loadParamsCaptor.getValue().getUrl());
        verifyNoMoreInteractions(mTab);
    }

    @Test
    public void loadPageInCurrentTab_openNewTabIfNonInteractable() {
        doReturn(false).when(mTab).isUserInteractable();
        mDelegate.loadPageInCurrentTab("url");

        verify(mTab, times(1)).isUserInteractable();
        verifyNoMoreInteractions(mTab);
        verify(mMockOpenUrl).accept("url");
    }

    @Test
    public void loadPageInCurrentTab_openNewTabIfNoTabs() {
        mTabReference.set(null);
        mDelegate.loadPageInCurrentTab("url");
        verifyNoMoreInteractions(mTab);
        verify(mMockOpenUrl).accept("url");
    }

    @Test
    public void openQuickDeleteDialog() {
        mDelegate.handleClearBrowsingData();
        verify(mMockOpenQuickDeleteDialog).run();
    }

    @Test
    public void startActivity_targetSelf() {
        shadowOf((Application) ApplicationProvider.getApplicationContext()).checkActivities(true);
        Intent i = new Intent();
        i.setClass(mContext, TestActivity.class);
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        assertTrue(IntentUtils.intentTargetsSelf(i));
        assertTrue(mDelegate.startActivity(i));
        // Added during intent invocation.
        assertTrue(i.hasExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA));
    }

    @Test
    public void startActivity_targetOther() {
        // Do not arm the package resolution.
        shadowOf((Application) ApplicationProvider.getApplicationContext()).checkActivities(false);
        Intent i = new Intent("some magic here");
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        assertFalse(IntentUtils.intentTargetsSelf(i));
        assertTrue(mDelegate.startActivity(i));
        // Might be added during intent invocation.
        assertFalse(i.hasExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA));
    }

    @Test
    public void startActivity_failure() {
        shadowOf((Application) ApplicationProvider.getApplicationContext()).checkActivities(true);
        Intent i = new Intent("some magic here");
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        assertFalse(mDelegate.startActivity(i));
    }

    @Test
    public void switchToTab_noTabManager() {
        assertFalse(mDelegate.switchToTab(1));
    }

    @Test
    public void switchToTab_noTargetTab() {
        mTabManagerSupplier.set(mTabManager);
        doReturn(null).when(mTabManager).getTabById(1);
        assertFalse(mDelegate.switchToTab(1));
    }

    @Test
    public void switchToTab_noWindowAndroid() {
        mTabManagerSupplier.set(mTabManager);
        doReturn(mTab).when(mTabManager).getTabById(1);
        doReturn(null).when(mTab).getWindowAndroid();
        assertFalse(mDelegate.switchToTab(1));
    }

    @Test
    public void switchToTab_tabAttachedToStoppedActivity() {
        // We have a tab, and tab manager. The tab is part of the stopped activity.
        mTabManagerSupplier.set(mTabManager);
        doReturn(mTab).when(mTabManager).getTabById(1);
        doReturn(mMockWindowAndroid).when(mTab).getWindowAndroid();
        doReturn(ActivityState.STOPPED).when(mMockWindowAndroid).getActivityState();
        assertTrue(mDelegate.switchToTab(1));
    }

    @Test
    public void switchToTab_noTabModelForTab() {
        // We have a tab, and tab manager. The tab is part of the running activity.
        // The tab is not a part of the model though (eg. it has just been closed).
        // https://crbug.com/1300447
        mTabManagerSupplier.set(mTabManager);
        doReturn(mTab).when(mTabManager).getTabById(1);
        doReturn(mMockWindowAndroid).when(mTab).getWindowAndroid();
        doReturn(ActivityState.RESUMED).when(mMockWindowAndroid).getActivityState();
        doReturn(null).when(mTabManager).getTabModelForTab(any());
        assertFalse(mDelegate.switchToTab(1));
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void switchToTab_invalidTabModelAssociation() {
        // We have a tab, and tab manager. The tab is part of the running activity.
        // The tab reports association with an existing model, but the model thinks otherwise.
        // https://crbug.com/1300447
        mTabManagerSupplier.set(mTabManager);
        doReturn(mTab).when(mTabManager).getTabById(1);
        doReturn(mMockWindowAndroid).when(mTab).getWindowAndroid();
        doReturn(ActivityState.RESUMED).when(mMockWindowAndroid).getActivityState();
        doReturn(mTabModel).when(mTabManager).getTabModelForTab(any());

        // Make sure that this indeed returns no association.
        assertEquals(
                TabModel.INVALID_TAB_INDEX, TabModelUtils.getTabIndexById(mTabModel, mTab.getId()));
        assertFalse(mDelegate.switchToTab(1));
    }

    @Test
    public void switchToTab_validTabModelAssociation() {
        // We have a tab, and tab manager. The tab is part of the running activity.
        // The tab reports association with an existing model; the model confirms this.
        mTabManagerSupplier.set(mTabManager);
        doReturn(mTab).when(mTabManager).getTabById(1);
        doReturn(mMockWindowAndroid).when(mTab).getWindowAndroid();
        doReturn(ActivityState.RESUMED).when(mMockWindowAndroid).getActivityState();
        doReturn(mTabModel).when(mTabManager).getTabModelForTab(any());
        doReturn(1).when(mTabModel).getCount();
        doAnswer(inv -> List.of(mTab).iterator()).when(mTabModel).iterator();
        doReturn(1).when(mTab).getId();
        assertTrue(mDelegate.switchToTab(1));
    }
}
