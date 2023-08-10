// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.page_insights.PageInsightsMediator.PAGE_INSIGHTS_CAN_AUTOTRIGGER_AFTER_END;

import android.content.Context;
import android.view.LayoutInflater;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.ExpandedSheetHelper;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;

import java.util.concurrent.TimeUnit;

/**
 * Unit tests for {@link PageInsightsMediator}.
 */
@LooperMode(Mode.PAUSED)
@RunWith(BaseRobolectricTestRunner.class)
public class PageInsightsMediatorTest {
    @Mock
    private LayoutInflater mLayoutInflater;
    @Mock
    private ObservableSupplier<Tab> mMockTabProvider;
    @Mock
    private ManagedBottomSheetController mBottomSheetController;
    @Mock
    private BottomSheetController mBottomUiController;
    @Mock
    private ExpandedSheetHelper mExpandedSheetHelper;
    @Mock
    private BrowserControlsStateProvider mControlsStateProvider;
    @Mock
    private BrowserControlsSizer mBrowserControlsSizer;
    @Mock
    private Tab mTab;

    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer>
            mBrowserControlsStateProviderObserver;

    private ShadowLooper mShadowLooper;

    private PageInsightsMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Context mContext = ContextUtils.getApplicationContext();
        mShadowLooper = ShadowLooper.shadowMainLooper();
        when(mControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(1.0f);
        mMediator = new PageInsightsMediator(mContext, mMockTabProvider, mBottomSheetController,
                mBottomUiController, mExpandedSheetHelper, mControlsStateProvider,
                mBrowserControlsSizer, () -> true);
        verify(mControlsStateProvider).addObserver(mBrowserControlsStateProviderObserver.capture());
    }

    @Test
    @MediumTest
    public void testAutoTrigger_doesNotTriggerImmediately() throws Exception {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        testValues.addFieldTrialParamOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                PAGE_INSIGHTS_CAN_AUTOTRIGGER_AFTER_END, "2000");
        FeatureList.setTestValues(testValues);

        mMediator.onLoadStopped(mTab, true);
        mBrowserControlsStateProviderObserver.getValue().onControlsOffsetChanged(0, 70, 0, 0, true);

        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    @MediumTest
    public void testAutoTrigger_notEnoughDuration_doesNotTrigger() throws Exception {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        testValues.addFieldTrialParamOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                PAGE_INSIGHTS_CAN_AUTOTRIGGER_AFTER_END, "2000");
        FeatureList.setTestValues(testValues);

        mMediator.onLoadStopped(mTab, true);
        mShadowLooper.idleFor(250, TimeUnit.MILLISECONDS);
        mBrowserControlsStateProviderObserver.getValue().onControlsOffsetChanged(0, 70, 0, 0, true);

        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    @MediumTest
    public void testAutoTrigger_enoughDuration_showsBottomSheet() throws Exception {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        testValues.addFieldTrialParamOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                PAGE_INSIGHTS_CAN_AUTOTRIGGER_AFTER_END, "2000");
        FeatureList.setTestValues(testValues);

        mMediator.onLoadStopped(mTab, true);

        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());

        mShadowLooper.idleFor(2500, TimeUnit.MILLISECONDS);
        mBrowserControlsStateProviderObserver.getValue().onControlsOffsetChanged(0, 70, 0, 0, true);

        verify(mBottomSheetController, times(1)).requestShowContent(any(), anyBoolean());
    }
}
