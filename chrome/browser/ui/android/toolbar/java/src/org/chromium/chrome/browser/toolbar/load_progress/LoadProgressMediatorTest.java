// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.load_progress;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.MathUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.toolbar.load_progress.LoadProgressProperties.CompletionState;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Unit tests for LoadProgressMediator. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class LoadProgressMediatorTest {
    private static final GURL URL_1 = new GURL("http://starting.url");
    private static final GURL NATIVE_PAGE_URL = new GURL("chrome-native://newtab");

    @Mock
    private Tab mTab;
    @Mock
    private Tab mTab2;

    @Captor
    public ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private PropertyModel mModel;
    private LoadProgressMediator mMediator;
    private TabObserver mTabObserver;
    private ObservableSupplierImpl<Tab> mTabSupplier;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel = new PropertyModel(LoadProgressProperties.ALL_KEYS);
        when(mTab.getUrl()).thenReturn(URL_1);
    }

    private void initMediator() {
        // ObservableSupplierImpl needs initialization in UI thread.
        mTabSupplier = new ObservableSupplierImpl<>();
        mMediator = new LoadProgressMediator(mTabSupplier, mModel, false);
        mTabSupplier.set(mTab);
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        mTabObserver = mTabObserverCaptor.getValue();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void loadRegularPage() {
        initMediator();
        assertEquals(mModel.get(LoadProgressProperties.COMPLETION_STATE),
                CompletionState.FINISHED_DONT_ANIMATE);

        NavigationHandle navigation =
                new NavigationHandle(0, URL_1, true, false, false, null, null);
        mTabObserver.onDidStartNavigation(mTab, navigation);
        assertEquals(
                mModel.get(LoadProgressProperties.COMPLETION_STATE), CompletionState.UNFINISHED);
        assertEquals(mModel.get(LoadProgressProperties.PROGRESS),
                LoadProgressMediator.MINIMUM_LOAD_PROGRESS, MathUtils.EPSILON);

        mTabObserver.onLoadProgressChanged(mTab, 0.1f);
        assertEquals(mModel.get(LoadProgressProperties.PROGRESS), 0.1f, MathUtils.EPSILON);

        mTabObserver.onLoadProgressChanged(mTab, 1.0f);
        assertEquals(mModel.get(LoadProgressProperties.PROGRESS), 1.0f, MathUtils.EPSILON);
        assertEquals(mModel.get(LoadProgressProperties.COMPLETION_STATE),
                CompletionState.FINISHED_DO_ANIMATE);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void switchToLoadingTab() {
        initMediator();
        doReturn(true).when(mTab2).isLoading();
        doReturn(0.1f).when(mTab2).getProgress();
        mTabSupplier.set(mTab2);
        verify(mTab2, times(1)).addObserver(any());

        assertEquals(
                mModel.get(LoadProgressProperties.COMPLETION_STATE), CompletionState.UNFINISHED);
        assertEquals(mModel.get(LoadProgressProperties.PROGRESS), 0.1f, MathUtils.EPSILON);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void switchToLoadedTab() {
        initMediator();
        NavigationHandle navigation =
                new NavigationHandle(0, URL_1, true, false, false, null, null);
        mTabObserver.onDidStartNavigation(mTab, navigation);
        assertEquals(
                mModel.get(LoadProgressProperties.COMPLETION_STATE), CompletionState.UNFINISHED);
        assertEquals(mModel.get(LoadProgressProperties.PROGRESS),
                LoadProgressMediator.MINIMUM_LOAD_PROGRESS, MathUtils.EPSILON);

        mTabSupplier.set(mTab2);
        verify(mTab2, times(1)).addObserver(any());
        assertEquals(mModel.get(LoadProgressProperties.COMPLETION_STATE),
                CompletionState.FINISHED_DONT_ANIMATE);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void loadNativePage() {
        initMediator();
        doReturn(0.1f).when(mTab).getProgress();
        NavigationHandle navigation =
                new NavigationHandle(0, URL_1, true, false, false, null, null);
        mTabObserver.onDidStartNavigation(mTab, navigation);
        assertEquals(
                mModel.get(LoadProgressProperties.COMPLETION_STATE), CompletionState.UNFINISHED);
        assertEquals(mModel.get(LoadProgressProperties.PROGRESS), 0.1f, MathUtils.EPSILON);

        navigation = new NavigationHandle(0, NATIVE_PAGE_URL, true, false, false, null, null);
        mTabObserver.onDidStartNavigation(mTab, navigation);
        assertEquals(mModel.get(LoadProgressProperties.COMPLETION_STATE),
                CompletionState.FINISHED_DONT_ANIMATE);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void switchToTabWithNativePage() {
        initMediator();
        NavigationHandle navigation =
                new NavigationHandle(0, URL_1, true, false, false, null, null);
        mTabObserver.onDidStartNavigation(mTab, navigation);
        assertEquals(
                mModel.get(LoadProgressProperties.COMPLETION_STATE), CompletionState.UNFINISHED);
        assertEquals(mModel.get(LoadProgressProperties.PROGRESS),
                LoadProgressMediator.MINIMUM_LOAD_PROGRESS, MathUtils.EPSILON);

        when(mTab2.getUrl()).thenReturn(NATIVE_PAGE_URL);
        mTabSupplier.set(mTab2);
        verify(mTab2, times(1)).addObserver(any());
        assertEquals(mModel.get(LoadProgressProperties.COMPLETION_STATE),
                CompletionState.FINISHED_DONT_ANIMATE);
        assertEquals(mModel.get(LoadProgressProperties.PROGRESS),
                LoadProgressMediator.MINIMUM_LOAD_PROGRESS, MathUtils.EPSILON);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void pageCrashes() {
        initMediator();
        NavigationHandle navigation =
                new NavigationHandle(0, URL_1, true, false, false, null, null);
        mTabObserver.onDidStartNavigation(mTab, navigation);
        assertEquals(
                mModel.get(LoadProgressProperties.COMPLETION_STATE), CompletionState.UNFINISHED);
        assertEquals(mModel.get(LoadProgressProperties.PROGRESS),
                LoadProgressMediator.MINIMUM_LOAD_PROGRESS, MathUtils.EPSILON);

        mTabObserver.onCrash(mTab);
        assertEquals(mModel.get(LoadProgressProperties.COMPLETION_STATE),
                CompletionState.FINISHED_DONT_ANIMATE);
        assertEquals(mModel.get(LoadProgressProperties.PROGRESS),
                LoadProgressMediator.MINIMUM_LOAD_PROGRESS, MathUtils.EPSILON);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSwapWebContents() {
        initMediator();
        assertEquals(mModel.get(LoadProgressProperties.COMPLETION_STATE),
                CompletionState.FINISHED_DONT_ANIMATE);
        mTabObserver.onWebContentsSwapped(mTab, true, true);
        assertEquals(
                mModel.get(LoadProgressProperties.COMPLETION_STATE), CompletionState.UNFINISHED);
        assertEquals(mModel.get(LoadProgressProperties.PROGRESS), 0, MathUtils.EPSILON);
        float expectedProgress = LoadProgressSimulator.PROGRESS_INCREMENT;
        while (expectedProgress < 1.0f + LoadProgressSimulator.PROGRESS_INCREMENT) {
            final float nextExpectedProgress = expectedProgress;
            CriteriaHelper.pollUiThreadNested(() -> {
                Criteria.checkThat((double) mModel.get(LoadProgressProperties.PROGRESS),
                        Matchers.closeTo(nextExpectedProgress, MathUtils.EPSILON));
            }, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL, 0);
            expectedProgress += LoadProgressSimulator.PROGRESS_INCREMENT;
        }

        assertEquals(mModel.get(LoadProgressProperties.COMPLETION_STATE),
                CompletionState.FINISHED_DO_ANIMATE);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSameDocumentLoad_afterFinishedLoading() {
        initMediator();
        GURL gurl = URL_1;
        assertEquals(mModel.get(LoadProgressProperties.COMPLETION_STATE),
                CompletionState.FINISHED_DONT_ANIMATE);

        NavigationHandle navigation = new NavigationHandle(0, gurl, true, false, false, null, null);
        mTabObserver.onDidStartNavigation(mTab, navigation);
        mTabObserver.onLoadProgressChanged(mTab, 1.0f);
        assertEquals(mModel.get(LoadProgressProperties.PROGRESS), 1.0f, MathUtils.EPSILON);
        assertEquals(mModel.get(LoadProgressProperties.COMPLETION_STATE),
                CompletionState.FINISHED_DO_ANIMATE);
        NavigationHandle sameDocNav = new NavigationHandle(0, gurl, true, true, false, null, null);
        mTabObserver.onDidStartNavigation(mTab, sameDocNav);

        assertEquals(mModel.get(LoadProgressProperties.PROGRESS), 1.0f, MathUtils.EPSILON);
        assertEquals(mModel.get(LoadProgressProperties.COMPLETION_STATE),
                CompletionState.FINISHED_DO_ANIMATE);
    }
}
