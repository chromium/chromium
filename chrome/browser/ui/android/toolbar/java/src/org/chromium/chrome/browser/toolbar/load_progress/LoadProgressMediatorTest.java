// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.load_progress;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Looper;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.MathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Criteria;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.toolbar.load_progress.LoadProgressProperties.CompletionState;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for LoadProgressMediator. */
@RunWith(BaseRobolectricTestRunner.class)
public class LoadProgressMediatorTest {
    private static final GURL URL_1 = JUnitTestGURLs.EXAMPLE_URL;
    private static final GURL NATIVE_PAGE_URL = JUnitTestGURLs.NTP_URL;

    @Mock private Tab mTab;
    @Mock private Tab mTab2;

    @Captor public ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private PropertyModel mModel;
    private LoadProgressMediator mMediator;
    private TabObserver mTabObserver;
    private ObservableSupplierImpl<Tab> mTabSupplier;
    private ShadowLooper mShadowLooper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new PropertyModel(LoadProgressProperties.ALL_KEYS));
        when(mTab.getUrl()).thenReturn(URL_1);
        mShadowLooper = Shadows.shadowOf(Looper.getMainLooper());
    }

    private void initMediator() {
        // ObservableSupplierImpl needs initialization in UI thread.
        mTabSupplier = new ObservableSupplierImpl<>();
        mMediator = new LoadProgressMediator(mTabSupplier, mModel);
        mTabSupplier.set(mTab);
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        mTabObserver = mTabObserverCaptor.getValue();
    }

    @Test
    @SmallTest
    public void loadRegularPage() {
        initMediator();
        assertEquals(
                CompletionState.FINISHED_DONT_ANIMATE,
                mModel.get(LoadProgressProperties.COMPLETION_STATE));

        NavigationHandle navigation =
                NavigationHandle.createForTesting(
                        URL_1,
                        /* isRendererInitiated= */ false,
                        /* pageTransition= */ 0,
                        /* hasUserGesture= */ false);
        mTabObserver.onDidStartNavigationInPrimaryMainFrame(mTab, navigation);
        assertEquals(
                CompletionState.UNFINISHED, mModel.get(LoadProgressProperties.COMPLETION_STATE));
        assertEquals(
                LoadProgressMediator.MINIMUM_LOAD_PROGRESS,
                mModel.get(LoadProgressProperties.PROGRESS),
                MathUtils.EPSILON);

        mTabObserver.onLoadProgressChanged(mTab, 0.1f);
        assertEquals(0.1f, mModel.get(LoadProgressProperties.PROGRESS), MathUtils.EPSILON);

        mTabObserver.onLoadProgressChanged(mTab, 1.0f);
        assertEquals(1.0f, mModel.get(LoadProgressProperties.PROGRESS), MathUtils.EPSILON);
        assertEquals(
                CompletionState.FINISHED_DO_ANIMATE,
                mModel.get(LoadProgressProperties.COMPLETION_STATE));
    }

    @Test
    @SmallTest
    public void switchToLoadingTab() {
        initMediator();
        doReturn(true).when(mTab2).isLoading();
        doReturn(0.1f).when(mTab2).getProgress();
        mTabSupplier.set(mTab2);
        verify(mTab2, times(1)).addObserver(any());

        assertEquals(
                CompletionState.UNFINISHED, mModel.get(LoadProgressProperties.COMPLETION_STATE));
        assertEquals(0.1f, mModel.get(LoadProgressProperties.PROGRESS), MathUtils.EPSILON);
    }

    @Test
    @SmallTest
    public void switchToLoadedTab() {
        initMediator();
        NavigationHandle navigation =
                NavigationHandle.createForTesting(
                        URL_1,
                        /* isRendererInitiated= */ false,
                        /* pageTransition= */ 0,
                        /* hasUserGesture= */ false);
        mTabObserver.onDidStartNavigationInPrimaryMainFrame(mTab, navigation);
        assertEquals(
                CompletionState.UNFINISHED, mModel.get(LoadProgressProperties.COMPLETION_STATE));
        assertEquals(
                LoadProgressMediator.MINIMUM_LOAD_PROGRESS,
                mModel.get(LoadProgressProperties.PROGRESS),
                MathUtils.EPSILON);

        mTabSupplier.set(mTab2);
        verify(mTab2, times(1)).addObserver(any());
        assertEquals(
                CompletionState.FINISHED_DONT_ANIMATE,
                mModel.get(LoadProgressProperties.COMPLETION_STATE));
    }

    @Test
    @SmallTest
    public void loadNativePage() {
        initMediator();
        doReturn(0.1f).when(mTab).getProgress();
        NavigationHandle navigation =
                NavigationHandle.createForTesting(
                        URL_1,
                        /* isRendererInitiated= */ false,
                        /* pageTransition= */ 0,
                        /* hasUserGesture= */ false);
        mTabObserver.onDidStartNavigationInPrimaryMainFrame(mTab, navigation);
        assertEquals(
                CompletionState.UNFINISHED, mModel.get(LoadProgressProperties.COMPLETION_STATE));
        assertEquals(0.1f, mModel.get(LoadProgressProperties.PROGRESS), MathUtils.EPSILON);

        navigation =
                NavigationHandle.createForTesting(
                        NATIVE_PAGE_URL,
                        /* isRendererInitiated= */ false,
                        /* pageTransition= */ 0,
                        /* hasUserGesture= */ false);
        mTabObserver.onDidStartNavigationInPrimaryMainFrame(mTab, navigation);
        assertEquals(
                CompletionState.FINISHED_DONT_ANIMATE,
                mModel.get(LoadProgressProperties.COMPLETION_STATE));
    }

    @Test
    @SmallTest
    public void switchToTabWithNativePage() {
        initMediator();
        NavigationHandle navigation =
                NavigationHandle.createForTesting(
                        URL_1,
                        /* isRendererInitiated= */ false,
                        /* pageTransition= */ 0,
                        /* hasUserGesture= */ false);
        mTabObserver.onDidStartNavigationInPrimaryMainFrame(mTab, navigation);
        assertEquals(
                CompletionState.UNFINISHED, mModel.get(LoadProgressProperties.COMPLETION_STATE));
        assertEquals(
                LoadProgressMediator.MINIMUM_LOAD_PROGRESS,
                mModel.get(LoadProgressProperties.PROGRESS),
                MathUtils.EPSILON);

        when(mTab2.getUrl()).thenReturn(NATIVE_PAGE_URL);
        mTabSupplier.set(mTab2);
        verify(mTab2, times(1)).addObserver(any());
        assertEquals(
                CompletionState.FINISHED_DONT_ANIMATE,
                mModel.get(LoadProgressProperties.COMPLETION_STATE));
        assertEquals(
                LoadProgressMediator.MINIMUM_LOAD_PROGRESS,
                mModel.get(LoadProgressProperties.PROGRESS),
                MathUtils.EPSILON);
    }

    @Test
    @SmallTest
    public void pageCrashes() {
        initMediator();

        NavigationHandle navigation =
                NavigationHandle.createForTesting(
                        URL_1,
                        /* isRendererInitiated= */ false,
                        /* pageTransition= */ 0,
                        /* hasUserGesture= */ false);
        mTabObserver.onDidStartNavigationInPrimaryMainFrame(mTab, navigation);
        assertEquals(
                CompletionState.UNFINISHED, mModel.get(LoadProgressProperties.COMPLETION_STATE));
        assertEquals(
                LoadProgressMediator.MINIMUM_LOAD_PROGRESS,
                mModel.get(LoadProgressProperties.PROGRESS),
                MathUtils.EPSILON);

        mTabObserver.onCrash(mTab);
        assertEquals(
                CompletionState.FINISHED_DONT_ANIMATE,
                mModel.get(LoadProgressProperties.COMPLETION_STATE));
        assertEquals(
                LoadProgressMediator.MINIMUM_LOAD_PROGRESS,
                mModel.get(LoadProgressProperties.PROGRESS),
                MathUtils.EPSILON);
    }

    @Test
    @SmallTest
    public void testSwapWebContents() {
        initMediator();
        assertEquals(
                CompletionState.FINISHED_DONT_ANIMATE,
                mModel.get(LoadProgressProperties.COMPLETION_STATE));
        // Swap web contents after loading started and finished. As loading already happened we
        // simulate the load events.
        mTabObserver.onWebContentsSwapped(mTab, true, true);
        assertEquals(
                CompletionState.UNFINISHED, mModel.get(LoadProgressProperties.COMPLETION_STATE));
        assertEquals(0, mModel.get(LoadProgressProperties.PROGRESS), MathUtils.EPSILON);

        // Ensure load events are simulated as expected.
        float expectedProgress = LoadProgressSimulator.PROGRESS_INCREMENT;
        while (expectedProgress < 1.0f + LoadProgressSimulator.PROGRESS_INCREMENT) {
            mShadowLooper.runOneTask();
            final float nextExpectedProgress = expectedProgress;
            Criteria.checkThat(
                    (double) mModel.get(LoadProgressProperties.PROGRESS),
                    Matchers.closeTo(nextExpectedProgress, MathUtils.EPSILON));
            expectedProgress += LoadProgressSimulator.PROGRESS_INCREMENT;
        }

        assertEquals(
                CompletionState.FINISHED_DO_ANIMATE,
                mModel.get(LoadProgressProperties.COMPLETION_STATE));
    }

    @Test
    @SmallTest
    public void testSameDocumentLoad_afterFinishedLoading() {
        initMediator();
        GURL gurl = URL_1;
        assertEquals(
                CompletionState.FINISHED_DONT_ANIMATE,
                mModel.get(LoadProgressProperties.COMPLETION_STATE));

        NavigationHandle navigation =
                NavigationHandle.createForTesting(
                        gurl,
                        /* isRendererInitiated= */ false,
                        /* pageTransition= */ 0,
                        /* hasUserGesture= */ false);
        mTabObserver.onDidStartNavigationInPrimaryMainFrame(mTab, navigation);
        mTabObserver.onLoadProgressChanged(mTab, 1.0f);
        assertEquals(1.0f, mModel.get(LoadProgressProperties.PROGRESS), MathUtils.EPSILON);
        assertEquals(
                CompletionState.FINISHED_DO_ANIMATE,
                mModel.get(LoadProgressProperties.COMPLETION_STATE));
        NavigationHandle sameDocNav =
                NavigationHandle.createForTesting(
                        gurl,
                        /* isInPrimaryMainFrame= */ true,
                        /* isSameDocument= */ true,
                        /* isRendererInitiated= */ false,
                        /* pageTransition= */ 0,
                        /* hasUserGesture= */ false,
                        /* isReload= */ false);
        mTabObserver.onDidStartNavigationInPrimaryMainFrame(mTab, sameDocNav);

        assertEquals(1.0f, mModel.get(LoadProgressProperties.PROGRESS), MathUtils.EPSILON);
        assertEquals(
                CompletionState.FINISHED_DO_ANIMATE,
                mModel.get(LoadProgressProperties.COMPLETION_STATE));
    }
}
