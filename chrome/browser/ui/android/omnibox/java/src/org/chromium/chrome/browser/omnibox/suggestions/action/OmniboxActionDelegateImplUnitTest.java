// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Context;
import android.content.Intent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.TestActivity;

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
    private AtomicReference<Tab> mTabReference = new AtomicReference<>();
    private Context mContext;
    private OmniboxActionDelegateImpl mDelegate;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mTabReference.set(mTab);
        mDelegate =
                new OmniboxActionDelegateImpl(
                        mContext,
                        () -> mTabReference.get(),
                        mMockOpenUrl,
                        mMockOpenIncognitoPage,
                        mMockOpenPasswordSettings,
                        mMockOpenQuickDeleteDialog);
        SettingsNavigationFactory.setInstanceForTesting(mMockSettingsNavigation);
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
    @DisableFeatures(ChromeFeatureList.QUICK_DELETE_FOR_ANDROID)
    public void openClearBrowsingData() {
        mDelegate.handleClearBrowsingData();
        verify(mMockSettingsNavigation)
                .startSettings(mContext, SettingsFragment.CLEAR_BROWSING_DATA_ADVANCED_PAGE);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.QUICK_DELETE_FOR_ANDROID)
    public void openQuickDeleteDialog() {
        mDelegate.handleClearBrowsingData();
        verify(mMockOpenQuickDeleteDialog).run();
    }

    @Test
    public void startActivity_targetSelf() {
        ShadowApplication.getInstance().checkActivities(true);
        Intent i = new Intent();
        i.setClass(mContext, TestActivity.class);
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        assertTrue(IntentUtils.intentTargetsSelf(mContext, i));
        assertTrue(mDelegate.startActivity(i));
        // Added during intent invocation.
        assertTrue(i.hasExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA));
    }

    @Test
    public void startActivity_targetOther() {
        // Do not arm the package resolution.
        ShadowApplication.getInstance().checkActivities(false);
        Intent i = new Intent("some magic here");
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        assertFalse(IntentUtils.intentTargetsSelf(mContext, i));
        assertTrue(mDelegate.startActivity(i));
        // Might be added during intent invocation.
        assertFalse(i.hasExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA));
    }

    @Test
    public void startActivity_failure() {
        ShadowApplication.getInstance().checkActivities(true);
        Intent i = new Intent("some magic here");
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        assertFalse(mDelegate.startActivity(i));
    }
}
