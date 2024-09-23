// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for the top toolbar overlay's mediator (composited version of the top toolbar). */
@RunWith(BaseRobolectricTestRunner.class)
public class TopToolbarOverlayMediatorTest {
    private TopToolbarOverlayMediator mMediator;
    private PropertyModel mModel;

    @Mock private Context mContext;

    @Mock private LayoutStateProvider mLayoutStateProvider;

    @Mock private BrowserControlsStateProvider mBrowserControlsProvider;

    @Mock private TopUiThemeColorProvider mTopUiThemeColorProvider;

    @Mock private Tab mTab;

    @Mock private Tab mTab2;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer> mBrowserControlsObserverCaptor;

    @Captor private ArgumentCaptor<LayoutStateProvider.LayoutStateObserver> mLayoutObserverCaptor;

    @Mock private ObservableSupplier<Tab> mTabSupplier;

    @Captor private ArgumentCaptor<Callback<Tab>> mActivityTabObserverCaptor;

    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);

        TopToolbarOverlayMediator.setToolbarBackgroundColorForTesting(Color.RED);
        TopToolbarOverlayMediator.setUrlBarColorForTesting(Color.BLUE);
        TopToolbarOverlayMediator.setIsTabletForTesting(false);

        mModel =
                new PropertyModel.Builder(TopToolbarOverlayProperties.ALL_KEYS)
                        .with(TopToolbarOverlayProperties.RESOURCE_ID, 0)
                        .with(TopToolbarOverlayProperties.URL_BAR_RESOURCE_ID, 0)
                        .with(TopToolbarOverlayProperties.CONTENT_OFFSET, 0)
                        .with(TopToolbarOverlayProperties.SHOW_SHADOW, true)
                        .with(
                                TopToolbarOverlayProperties.TOOLBAR_BACKGROUND_COLOR,
                                Color.TRANSPARENT)
                        .with(TopToolbarOverlayProperties.URL_BAR_COLOR, Color.TRANSPARENT)
                        .with(TopToolbarOverlayProperties.PROGRESS_BAR_INFO, null)
                        .build();

        when(mTabSupplier.get()).thenReturn(mTab);
        mMediator =
                new TopToolbarOverlayMediator(
                        mModel,
                        mContext,
                        mLayoutStateProvider,
                        (info) -> {},
                        mTabSupplier,
                        mBrowserControlsProvider,
                        mTopUiThemeColorProvider,
                        LayoutType.BROWSING,
                        false);

        mMediator.setIsAndroidViewVisible(true);

        // Ensure the observer is added to the initial tab.
        verify(mTabSupplier).addObserver(mActivityTabObserverCaptor.capture());
        setTabSupplierTab(mTab);

        verify(mTab).addObserver(mTabObserverCaptor.capture());

        verify(mBrowserControlsProvider).addObserver(mBrowserControlsObserverCaptor.capture());

        verify(mLayoutStateProvider).addObserver(mLayoutObserverCaptor.capture());

        mLayoutObserverCaptor.getValue().onStartedShowing(LayoutType.BROWSING);
    }

    /** Set the tab that will be returned by the supplier and trigger the observer event. */
    private void setTabSupplierTab(Tab tab) {
        when(mTabSupplier.get()).thenReturn(tab);
        mActivityTabObserverCaptor.getValue().onResult(tab);
    }

    @Test
    public void testShadowVisibility_browserControlsOffsets() {
        when(mBrowserControlsProvider.getBrowserControlHiddenRatio()).thenReturn(0.0f);
        mBrowserControlsObserverCaptor.getValue().onControlsOffsetChanged(0, 0, 0, 0, false, false);

        Assert.assertFalse(
                "Shadow should be invisible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));

        when(mBrowserControlsProvider.getBrowserControlHiddenRatio()).thenReturn(0.5f);
        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(100, 0, 0, 0, false, false);

        Assert.assertTrue(
                "Shadow should be visible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));
    }

    @Test
    public void testShadowVisibility_androidViewForceHidden() {
        mMediator.setIsAndroidViewVisible(true);

        Assert.assertFalse(
                "Shadow should be invisible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));

        mMediator.setIsAndroidViewVisible(false);

        Assert.assertTrue(
                "Shadow should be visible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
    public void testShadowVisibility_suppressToolbarCaptures() {
        mBrowserControlsObserverCaptor.getValue().onAndroidControlsVisibilityChanged(View.VISIBLE);
        Assert.assertFalse(
                "Shadow should be invisible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));

        mBrowserControlsObserverCaptor
                .getValue()
                .onAndroidControlsVisibilityChanged(View.INVISIBLE);
        Assert.assertTrue(
                "Shadow should be visible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
    public void testShadowVisibility_suppressToolbarCaptures_initialState() {
        when(mBrowserControlsProvider.getAndroidControlsVisibility()).thenReturn(View.VISIBLE);

        mMediator =
                new TopToolbarOverlayMediator(
                        mModel,
                        mContext,
                        mLayoutStateProvider,
                        (info) -> {},
                        mTabSupplier,
                        mBrowserControlsProvider,
                        mTopUiThemeColorProvider,
                        LayoutType.BROWSING,
                        false);
        mMediator.setIsAndroidViewVisible(true);

        Assert.assertFalse(
                "Shadow should be invisible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));
    }

    @Test
    public void testProgressUpdate_phone() {
        mModel.set(TopToolbarOverlayProperties.PROGRESS_BAR_INFO, null);

        mTabObserverCaptor.getValue().onLoadProgressChanged(mTab, 0.25f);

        Assert.assertNotNull(
                "The progress bar data should be populated.",
                mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO));

        // Ensure the progress is correct on tab switch.
        mTabObserverCaptor.getValue().onLoadProgressChanged(mTab, 0.f);
        setTabSupplierTab(mTab2);
    }

    @Test
    public void testProgressUpdate_tablet() {
        TopToolbarOverlayMediator.setIsTabletForTesting(true);
        mModel.set(TopToolbarOverlayProperties.PROGRESS_BAR_INFO, null);

        mTabObserverCaptor.getValue().onLoadProgressChanged(mTab, 0.25f);

        Assert.assertNull(
                "The progress bar data should be still be empty.",
                mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO));
    }

    @Test(expected = AssertionError.class)
    public void testManualVisibility_flagNotSet() {
        // If the manual visibility flag was not set in the constructor, expect as assert if someone
        // attempts to set it.
        mMediator.setManualVisibility(false);
    }

    @Test
    public void testManualVisibility() {
        mMediator.setVisibilityManuallyControlledForTesting(true);

        // Set the manual visibility to true and modify things that would otherwise change it.
        mMediator.setManualVisibility(true);
        mMediator.setIsAndroidViewVisible(true);

        Assert.assertTrue(
                "Shadow should be visible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));
        Assert.assertTrue(
                "View should be visible.", mModel.get(TopToolbarOverlayProperties.VISIBLE));

        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(100, 0, 0, 0, false, false);

        Assert.assertTrue(
                "Shadow should be visible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));
        Assert.assertTrue(
                "View should be visible.", mModel.get(TopToolbarOverlayProperties.VISIBLE));

        // Set the manual visibility to false and modify things that would otherwise change it.
        mMediator.setManualVisibility(false);

        // Note that an invisible view implies invisible shadow as well.
        Assert.assertFalse(
                "View should be invisible.", mModel.get(TopToolbarOverlayProperties.VISIBLE));

        mMediator.setIsAndroidViewVisible(false);

        Assert.assertFalse(
                "View should be invisible.", mModel.get(TopToolbarOverlayProperties.VISIBLE));

        mMediator.setVisibilityManuallyControlledForTesting(false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
    public void testAnonymize_suppressToolbarCaptures_nativePage() {
        Assert.assertFalse(mModel.get(TopToolbarOverlayProperties.ANONYMIZE));
        doReturn(true).when(mTab2).isNativePage();

        setTabSupplierTab(mTab2);

        Assert.assertTrue(mModel.get(TopToolbarOverlayProperties.ANONYMIZE));

        verify(mTab2).addObserver(mTabObserverCaptor.capture());
        doReturn(false).when(mTab2).isNativePage();
        mTabObserverCaptor.getValue().onContentChanged(mTab2);

        Assert.assertFalse(mModel.get(TopToolbarOverlayProperties.ANONYMIZE));
    }
}
