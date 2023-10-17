// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.toolbar.TabCountProvider.TabCountObserver;

/** Unit tests for {@link TabCountProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class TabCountProviderUnitTest {
    @Mock private TabModelSelector mMockTabModelSelector;
    @Mock private TabModelFilterProvider mMockFilterProvider;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    @Mock private TabModelFilter mMockTabModelFilter;
    @Mock private TabCountObserver mTabCountObserver;
    private TabCountProvider mProvider = new TabCountProvider();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMockTabModelSelector.getTabModelFilterProvider()).thenReturn(mMockFilterProvider);
        when(mMockTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mMockFilterProvider.getCurrentTabModelFilter()).thenReturn(mMockTabModelFilter);
    }

    @Test
    public void getTabCount_normalModel() {
        when(mMockTabModelSelector.isIncognitoSelected()).thenReturn(false);
        int tabCount = 10;
        when(mMockTabModelFilter.getTotalTabCount()).thenReturn(tabCount);
        mProvider.setTabModelSelector(mMockTabModelSelector);

        assertEquals(
                "Tab count from provider is not same as expected",
                tabCount,
                mProvider.getTabCount());
    }

    @Test
    public void getTabCount_incognitoModel() {
        when(mMockTabModelSelector.isIncognitoSelected()).thenReturn(true);
        int tabCount = 10;
        when(mMockTabModelFilter.getTotalTabCount()).thenReturn(tabCount);
        mProvider.setTabModelSelector(mMockTabModelSelector);

        assertEquals(
                "Tab count from provider is not same as expected",
                tabCount,
                mProvider.getTabCount());
    }

    @Test
    public void getTabCount_updatedViaSelectorObserver() {
        doNothing()
                .when(mMockTabModelSelector)
                .addObserver(mTabModelSelectorObserverCaptor.capture());
        when(mMockTabModelSelector.isIncognitoSelected()).thenReturn(false);
        mProvider.setTabModelSelector(mMockTabModelSelector);
        TabModelSelectorObserver observer = mTabModelSelectorObserverCaptor.getValue();

        int tabCount1 = 10;
        when(mMockTabModelFilter.getTotalTabCount()).thenReturn(tabCount1);
        observer.onTabModelSelected(null, null);
        assertEquals(
                "Tab count from provider is not same as expected",
                tabCount1,
                mProvider.getTabCount());

        int tabCount2 = 20;
        when(mMockTabModelFilter.getTotalTabCount()).thenReturn(tabCount2);
        observer.onTabStateInitialized();
        assertEquals(
                "Tab count from provider is not same as expected",
                tabCount2,
                mProvider.getTabCount());
    }

    @Test
    public void getTabCount_updatedViaModelObserver() {
        doNothing()
                .when(mMockFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        when(mMockTabModelSelector.isIncognitoSelected()).thenReturn(false);
        mProvider.setTabModelSelector(mMockTabModelSelector);
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        int tabCount1 = 10;
        when(mMockTabModelFilter.getTotalTabCount()).thenReturn(tabCount1);
        observer.didAddTab(null, 0, 0, false);
        assertEquals(
                "Tab count from provider is not same as expected",
                tabCount1,
                mProvider.getTabCount());

        int tabCount2 = 20;
        when(mMockTabModelFilter.getTotalTabCount()).thenReturn(tabCount2);
        observer.tabClosureUndone(null);
        assertEquals(
                "Tab count from provider is not same as expected",
                tabCount2,
                mProvider.getTabCount());

        int tabCount3 = 30;
        when(mMockTabModelFilter.getTotalTabCount()).thenReturn(tabCount3);
        observer.onFinishingTabClosure(null);
        assertEquals(
                "Tab count from provider is not same as expected",
                tabCount3,
                mProvider.getTabCount());

        int tabCount4 = 40;
        when(mMockTabModelFilter.getTotalTabCount()).thenReturn(tabCount4);
        observer.tabPendingClosure(null);
        assertEquals(
                "Tab count from provider is not same as expected",
                tabCount4,
                mProvider.getTabCount());

        int tabCount5 = 50;
        when(mMockTabModelFilter.getTotalTabCount()).thenReturn(tabCount5);
        observer.multipleTabsPendingClosure(null, false);
        assertEquals(
                "Tab count from provider is not same as expected",
                tabCount5,
                mProvider.getTabCount());

        int tabCount6 = 60;
        when(mMockTabModelFilter.getTotalTabCount()).thenReturn(tabCount6);
        observer.tabRemoved(null);
        assertEquals(
                "Tab count from provider is not same as expected",
                tabCount6,
                mProvider.getTabCount());

        int tabCount7 = 70;
        when(mMockTabModelFilter.getTotalTabCount()).thenReturn(tabCount7);
        observer.restoreCompleted();
        assertEquals(
                "Tab count from provider is not same as expected",
                tabCount7,
                mProvider.getTabCount());
    }

    @Test
    public void tabCountObservers() {
        mProvider.addObserver(mTabCountObserver);

        // Invoke update count via selector
        when(mMockTabModelSelector.isIncognitoSelected()).thenReturn(false);
        int tabCount = 10;
        when(mMockTabModelFilter.getTotalTabCount()).thenReturn(tabCount);
        mProvider.setTabModelSelector(mMockTabModelSelector);

        // verify observer invoked
        verify(mTabCountObserver).onTabCountChanged(10, false);
    }

    @Test
    public void tabCountObserverWithTrigger() {
        mProvider.setTabModelSelector(mMockTabModelSelector);
        when(mMockTabModelSelector.isIncognitoSelected()).thenReturn(false);
        int tabCount = 10;
        when(mMockTabModelFilter.getTotalTabCount()).thenReturn(tabCount);

        mProvider.addObserverAndTrigger(mTabCountObserver);

        // verify observer invoked
        verify(mTabCountObserver).onTabCountChanged(10, false);
    }
}
