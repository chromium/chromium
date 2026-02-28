// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.MathUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for the top toolbar overlay's mediator (composited version of the top toolbar). */
@RunWith(BaseRobolectricTestRunner.class)
public class TopToolbarOverlayMediatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private TopUiThemeColorProvider mTopUiThemeColorProvider;
    @Mock private Tab mTab;
    @Mock private Tab mTab2;
    @Mock private ToolbarProgressBar mProgressBar;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer> mBrowserControlsObserverCaptor;
    @Captor private ArgumentCaptor<LayoutStateProvider.LayoutStateObserver> mLayoutObserverCaptor;
    @Captor
    private ArgumentCaptor<ClipDrawableProgressBar.ProgressBarObserver> mProgressBarObserverCaptor;

    private final SettableMonotonicObservableSupplier<Tab> mTabSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<Integer> mBottomToolbarControlsOffsetSupplier =
            ObservableSuppliers.createNonNull(0);
    private final SettableNonNullObservableSupplier<Boolean> mSuppressToolbarSceneLayerSupplier =
            ObservableSuppliers.createNonNull(false);
    private final SettableMonotonicObservableSupplier<Long> mCaptureResourceIdSupplier =
            ObservableSuppliers.createMonotonic();
    private TopToolbarOverlayMediator mMediator;
    private PropertyModel mModel;

    @Before
    public void beforeTest() {
        TopToolbarOverlayMediator.setToolbarBackgroundColorForTesting(Color.RED);
        TopToolbarOverlayMediator.setUrlBarColorForTesting(Color.BLUE);
        TopToolbarOverlayMediator.setIsTabletForTesting(false);

        mModel =
                new PropertyModel.Builder(TopToolbarOverlayProperties.ALL_KEYS)
                        .with(TopToolbarOverlayProperties.RESOURCE_ID, 0)
                        .with(TopToolbarOverlayProperties.URL_BAR_RESOURCE_ID, 0)
                        .with(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET, 0)
                        .with(TopToolbarOverlayProperties.SHOW_SHADOW, true)
                        .with(
                                TopToolbarOverlayProperties.TOOLBAR_BACKGROUND_COLOR,
                                Color.TRANSPARENT)
                        .with(TopToolbarOverlayProperties.URL_BAR_COLOR, Color.TRANSPARENT)
                        .with(TopToolbarOverlayProperties.PROGRESS_BAR_INFO, null)
                        .build();

        mMediator =
                new TopToolbarOverlayMediator(
                        mModel,
                        mContext,
                        mLayoutStateProvider,
                        (info) -> {},
                        mTabSupplier,
                        mBrowserControlsStateProvider,
                        mTopUiThemeColorProvider,
                        mBottomToolbarControlsOffsetSupplier,
                        mSuppressToolbarSceneLayerSupplier,
                        LayoutType.BROWSING,
                        /* manualVisibilityControl= */ false,
                        mCaptureResourceIdSupplier,
                        mProgressBar);

        mMediator.setIsAndroidViewVisible(true);

        // Ensure the observer is added to the initial tab.
        setTabSupplierTab(mTab);

        verify(mProgressBar).addObserver(mProgressBarObserverCaptor.capture());
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        verify(mBrowserControlsStateProvider).addObserver(mBrowserControlsObserverCaptor.capture());
        verify(mLayoutStateProvider).addObserver(mLayoutObserverCaptor.capture());

        mLayoutObserverCaptor.getValue().onStartedShowing(LayoutType.BROWSING);
    }

    /** Set the tab that will be returned by the supplier and trigger the observer event. */
    private void setTabSupplierTab(Tab tab) {
        mTabSupplier.set(tab);
    }

    @Test
    public void testShadowVisibilityWhenControlsOffsetChanges() {
        when(mBrowserControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(0.0f);
        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(0, 0, false, 0, 0, false, false, false);

        assertTrue(
                "Shadow should be visible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));

        when(mBrowserControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(0.5f);
        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(100, 0, false, 0, 0, false, false, false);

        assertTrue(
                "Shadow should be visible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));
    }

    private void testShadowVisibility_androidViewForceHidden() {
        mMediator.setIsAndroidViewVisible(true);

        assertTrue(
                "Shadow should be visible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));

        mMediator.setIsAndroidViewVisible(false);

        assertTrue(
                "Shadow should be visible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));
    }

    @Test
    public void testShadowVisibility_androidViewForceHidden_bciv_enabled() {
        testShadowVisibility_androidViewForceHidden();
    }

    private void testShadowVisibility_suppressToolbarCaptures() {
        mBrowserControlsObserverCaptor.getValue().onAndroidControlsVisibilityChanged(View.VISIBLE);
        assertTrue(
                "Shadow should be visible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));

        mBrowserControlsObserverCaptor
                .getValue()
                .onAndroidControlsVisibilityChanged(View.INVISIBLE);
        assertTrue(
                "Shadow should be visible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));
    }

    @Test
    public void testShadowVisibility_suppressToolbarCaptures_bciv_enabled() {
        testShadowVisibility_suppressToolbarCaptures();
    }

    private void testShadowVisibility_suppressToolbarCaptures_initialState() {
        when(mBrowserControlsStateProvider.getAndroidControlsVisibility()).thenReturn(View.VISIBLE);

        mMediator =
                new TopToolbarOverlayMediator(
                        mModel,
                        mContext,
                        mLayoutStateProvider,
                        (info) -> {},
                        mTabSupplier,
                        mBrowserControlsStateProvider,
                        mTopUiThemeColorProvider,
                        mBottomToolbarControlsOffsetSupplier,
                        mSuppressToolbarSceneLayerSupplier,
                        LayoutType.BROWSING,
                        /* manualVisibilityControl= */ false,
                        mCaptureResourceIdSupplier,
                        mProgressBar);
        mMediator.setIsAndroidViewVisible(true);

        assertTrue(
                "Shadow should be visible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));
    }

    @Test
    public void testShadowVisibility_suppressToolbarCaptures_initialState_bciv_enabled() {
        testShadowVisibility_suppressToolbarCaptures_initialState();
    }

    @Test
    public void testProgressUpdate_phone_fromTabObserver() {
        mModel.set(TopToolbarOverlayProperties.PROGRESS_BAR_INFO, null);

        mTabObserverCaptor.getValue().onLoadProgressChanged(mTab, 0.25f);

        assertNull(
                "The progress bar data should be null.",
                mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO));

        // Ensure the progress is correct on tab switch.
        mTabObserverCaptor.getValue().onLoadProgressChanged(mTab, 0.f);
        setTabSupplierTab(mTab2);
    }

    @Test
    public void testProgressUpdate_phone_fromProgressBar() {
        mModel.set(TopToolbarOverlayProperties.PROGRESS_BAR_INFO, null);

        mProgressBarObserverCaptor.getValue().onVisibleProgressUpdated();

        assertNotNull(
                "The progress bar data should be populated.",
                mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO));
    }

    @Test
    public void testProgressUpdate_tablet() {
        TopToolbarOverlayMediator.setIsTabletForTesting(true);
        mModel.set(TopToolbarOverlayProperties.PROGRESS_BAR_INFO, null);

        mTabObserverCaptor.getValue().onLoadProgressChanged(mTab, 0.25f);

        assertNull(
                "The progress bar data should be still be empty.",
                mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO));
    }

    @Test
    public void testProgressUpdate_tablet_fromProgressBar() {
        TopToolbarOverlayMediator.setIsTabletForTesting(true);
        mModel.set(TopToolbarOverlayProperties.PROGRESS_BAR_INFO, null);

        mProgressBarObserverCaptor.getValue().onVisibleProgressUpdated();

        assertNotNull(
                "The progress bar data should not be empty.",
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

        assertTrue(
                "Shadow should be visible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));
        assertTrue("View should be visible.", mModel.get(TopToolbarOverlayProperties.VISIBLE));

        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(100, 0, false, 0, 0, false, false, false);

        assertTrue(
                "Shadow should be visible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));
        assertTrue("View should be visible.", mModel.get(TopToolbarOverlayProperties.VISIBLE));

        // Set the manual visibility to false and modify things that would otherwise change it.
        mMediator.setManualVisibility(false);

        // Note that an invisible view implies invisible shadow as well.
        assertFalse("View should be invisible.", mModel.get(TopToolbarOverlayProperties.VISIBLE));

        mMediator.setIsAndroidViewVisible(false);

        assertFalse("View should be invisible.", mModel.get(TopToolbarOverlayProperties.VISIBLE));

        mMediator.setVisibilityManuallyControlledForTesting(false);
    }

    @Test
    public void testAnonymize_suppressToolbarCaptures_nativePage() {
        assertFalse(mModel.get(TopToolbarOverlayProperties.ANONYMIZE));
        doReturn(true).when(mTab2).isNativePage();

        setTabSupplierTab(mTab2);

        assertTrue(mModel.get(TopToolbarOverlayProperties.ANONYMIZE));

        verify(mTab2).addObserver(mTabObserverCaptor.capture());
        doReturn(false).when(mTab2).isNativePage();
        mTabObserverCaptor.getValue().onContentChanged(mTab2);

        assertFalse(mModel.get(TopToolbarOverlayProperties.ANONYMIZE));
    }

    @Test
    // TODO(crbug.com/430058918): Reenable or add new test.
    @DisableFeatures(ChromeFeatureList.TOP_CONTROLS_REFACTOR_V2)
    public void testBottomToolbarOffset() {
        float height = 700.0f;
        mMediator.setViewportHeight(height);
        mBottomToolbarControlsOffsetSupplier.set(-40);

        doReturn(ControlsPosition.TOP).when(mBrowserControlsStateProvider).getControlsPosition();
        mBrowserControlsObserverCaptor.getValue().onControlsPositionChanged(ControlsPosition.TOP);
        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(0, 0, false, 30, 0, false, false, false);
        assertEquals(
                0.0f,
                mModel.get(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET),
                MathUtils.EPSILON);

        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsStateProvider).getControlsPosition();
        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsPositionChanged(ControlsPosition.BOTTOM);
        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(0, 0, false, 30, 0, false, false, false);
        assertEquals(
                height + mBottomToolbarControlsOffsetSupplier.get(),
                mModel.get(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET),
                MathUtils.EPSILON);

        float newHeight = 1700.0f;
        mMediator.setViewportHeight(newHeight);
        assertEquals(
                newHeight + mBottomToolbarControlsOffsetSupplier.get(),
                mModel.get(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET),
                MathUtils.EPSILON);

        mBottomToolbarControlsOffsetSupplier.set(-80);
        assertEquals(
                newHeight + mBottomToolbarControlsOffsetSupplier.get(),
                mModel.get(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET),
                MathUtils.EPSILON);
    }

    @Test
    public void testSuppressVisibility() {
        assertTrue("View should be visible.", mModel.get(TopToolbarOverlayProperties.VISIBLE));

        mSuppressToolbarSceneLayerSupplier.set(true);
        assertFalse("View should be gone.", mModel.get(TopToolbarOverlayProperties.VISIBLE));

        mSuppressToolbarSceneLayerSupplier.set(false);
        assertTrue("View should be visible.", mModel.get(TopToolbarOverlayProperties.VISIBLE));
    }

    @Test
    // TODO(crbug.com/430058918): Reenable or add new test.
    @DisableFeatures(ChromeFeatureList.TOP_CONTROLS_REFACTOR_V2)
    public void testTopToolbarOffset() {
        int offset = -10;
        int height = 150;
        doReturn(offset).when(mBrowserControlsStateProvider).getContentOffset();
        doReturn(height).when(mBrowserControlsStateProvider).getTopControlsHeight();

        doReturn(ControlsPosition.TOP).when(mBrowserControlsStateProvider).getControlsPosition();
        mBrowserControlsObserverCaptor.getValue().onControlsPositionChanged(ControlsPosition.TOP);

        assertEquals(
                0.0f,
                mModel.get(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET),
                MathUtils.EPSILON);

        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(0, 0, false, 0, 0, false, true, false);
        assertEquals(
                offset,
                mModel.get(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET),
                MathUtils.EPSILON);
        mModel.set(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET, 0);

        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(0, 0, false, 0, 0, false, false, true);
        assertEquals(
                offset,
                mModel.get(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET),
                MathUtils.EPSILON);
        mModel.set(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET, 0);

        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(0, 0, false, 0, 0, false, false, false);
        assertEquals(
                height,
                mModel.get(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET),
                MathUtils.EPSILON);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TOP_CONTROLS_REFACTOR_V2)
    public void testOffsetTagAndConstraintChanges() {
        BrowserControlsOffsetTagsInfo tagsInfo = new BrowserControlsOffsetTagsInfo();
        int offset = -10;
        doReturn(offset).when(mBrowserControlsStateProvider).getContentOffset();

        doReturn(ControlsPosition.TOP).when(mBrowserControlsStateProvider).getControlsPosition();
        mBrowserControlsObserverCaptor.getValue().onOffsetTagsInfoChanged(null, tagsInfo, 0, false);
        assertEquals(
                tagsInfo.getTopControlsOffsetTag(),
                mModel.get(TopToolbarOverlayProperties.TOOLBAR_OFFSET_TAG));
        assertEquals(0, (int) mModel.get(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET));
        mBrowserControlsObserverCaptor.getValue().onOffsetTagsInfoChanged(null, tagsInfo, 0, true);
        assertEquals(
                tagsInfo.getTopControlsOffsetTag(),
                mModel.get(TopToolbarOverlayProperties.TOOLBAR_OFFSET_TAG));
        assertEquals(offset, (int) mModel.get(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET));

        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsStateProvider).getControlsPosition();
        mBrowserControlsObserverCaptor.getValue().onOffsetTagsInfoChanged(null, tagsInfo, 0, false);
        assertEquals(
                tagsInfo.getBottomControlsOffsetTag(),
                mModel.get(TopToolbarOverlayProperties.TOOLBAR_OFFSET_TAG));
        assertEquals(offset, (int) mModel.get(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET));

        doReturn(ControlsPosition.NONE).when(mBrowserControlsStateProvider).getControlsPosition();
        mBrowserControlsObserverCaptor.getValue().onOffsetTagsInfoChanged(null, tagsInfo, 0, true);
        assertNull(mModel.get(TopToolbarOverlayProperties.TOOLBAR_OFFSET_TAG));
        assertEquals(offset, (int) mModel.get(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TOP_CONTROLS_REFACTOR,
        ChromeFeatureList.TOP_CONTROLS_REFACTOR_V2
    })
    public void testOffsetTagAndConstraintChanges_topControlsRefactor() {
        BrowserControlsOffsetTagsInfo originalOffsetTag = new BrowserControlsOffsetTagsInfo();
        int offset = -10;
        doReturn(offset).when(mBrowserControlsStateProvider).getContentOffset();

        mMediator.updateOffsetTag(originalOffsetTag);

        BrowserControlsOffsetTagsInfo newOffsetTag = new BrowserControlsOffsetTagsInfo();
        doReturn(ControlsPosition.TOP).when(mBrowserControlsStateProvider).getControlsPosition();
        mBrowserControlsObserverCaptor
                .getValue()
                .onOffsetTagsInfoChanged(null, newOffsetTag, 0, false);
        assertEquals(
                "Original offset tag should not changed by onOffsetTagsInfoChanged.",
                originalOffsetTag.getTopControlsOffsetTag(),
                mModel.get(TopToolbarOverlayProperties.TOOLBAR_OFFSET_TAG));

        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsStateProvider).getControlsPosition();
        mBrowserControlsObserverCaptor
                .getValue()
                .onOffsetTagsInfoChanged(null, newOffsetTag, 0, false);
        assertEquals(
                "Offset tag should be changed by onOffsetTagsInfoChanged for bottom controls.",
                newOffsetTag.getBottomControlsOffsetTag(),
                mModel.get(TopToolbarOverlayProperties.TOOLBAR_OFFSET_TAG));
    }

    @Test
    public void testDestroy() {
        assertTrue(mBottomToolbarControlsOffsetSupplier.hasObservers());
        assertTrue(mSuppressToolbarSceneLayerSupplier.hasObservers());
        verify(mLayoutStateProvider, never()).removeObserver(mLayoutObserverCaptor.getValue());
        verify(mBrowserControlsStateProvider, never())
                .removeObserver(mBrowserControlsObserverCaptor.getValue());
        assertTrue(mCaptureResourceIdSupplier.hasObservers());

        mMediator.destroy();

        assertFalse(mTabSupplier.hasObservers());
        assertFalse(mBottomToolbarControlsOffsetSupplier.hasObservers());
        assertFalse(mSuppressToolbarSceneLayerSupplier.hasObservers());
        verify(mLayoutStateProvider).removeObserver(mLayoutObserverCaptor.getValue());
        verify(mBrowserControlsStateProvider)
                .removeObserver(mBrowserControlsObserverCaptor.getValue());
        assertFalse(mCaptureResourceIdSupplier.hasObservers());
    }

    @Test
    public void testCaptureResourceId() {
        long captureResourceId = 1234;
        mCaptureResourceIdSupplier.set(captureResourceId);
        assertEquals(
                captureResourceId, mModel.get(TopToolbarOverlayProperties.CAPTURE_RESOURCE_ID));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TOP_CONTROLS_REFACTOR,
        ChromeFeatureList.TOP_CONTROLS_REFACTOR_V2
    })
    public void testContentOffset_topControlsRefactorEnabled() {
        int offset = -10;
        int height = 150;
        doReturn(offset).when(mBrowserControlsStateProvider).getContentOffset();
        doReturn(height).when(mBrowserControlsStateProvider).getTopControlsHeight();
        doReturn(ControlsPosition.TOP).when(mBrowserControlsStateProvider).getControlsPosition();
        mBrowserControlsObserverCaptor.getValue().onControlsPositionChanged(ControlsPosition.TOP);

        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(0, 0, false, 0, 0, false, false, false);
        assertEquals(
                TopToolbarOverlayMediator.INVALID_CONTENT_OFFSET,
                mModel.get(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET),
                MathUtils.EPSILON);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TOP_CONTROLS_REFACTOR,
        ChromeFeatureList.TOP_CONTROLS_REFACTOR_V2
    })
    public void testContentOffset_topControlsRefactorEnabled_manuallyControlled() {
        mMediator.setVisibilityManuallyControlledForTesting(true);

        int height = 150;
        doReturn(height).when(mBrowserControlsStateProvider).getTopControlsHeight();
        doReturn(ControlsPosition.TOP).when(mBrowserControlsStateProvider).getControlsPosition();
        mBrowserControlsObserverCaptor.getValue().onControlsPositionChanged(ControlsPosition.TOP);

        // When requestNewFrame is false, applyContentOffsetToModel receives getTopControlsHeight().
        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(
                        0, 0, false, 0, 0, false, /* requestNewFrame= */ false, false);
        assertEquals(
                (float) height,
                mModel.get(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET),
                MathUtils.EPSILON);

        // When requestNewFrame is true, applyContentOffsetToModel receives getContentOffset().
        int contentOffset = 200;
        doReturn(contentOffset).when(mBrowserControlsStateProvider).getContentOffset();
        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(
                        0, 0, false, 0, 0, false, /* requestNewFrame= */ true, false);
        assertEquals(
                (float) contentOffset,
                mModel.get(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET),
                MathUtils.EPSILON);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TOP_CONTROLS_REFACTOR,
        ChromeFeatureList.TOP_CONTROLS_REFACTOR_V2
    })
    public void testContentOffset_topControlsRefactorEnabled_ControlsAtBottom() {
        float height = 700.0f;
        mMediator.setViewportHeight(height);
        mBottomToolbarControlsOffsetSupplier.set(0);
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsStateProvider).getControlsPosition();

        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(0, 0, false, 0, 0, false, false, false);
        assertEquals(
                700.0f,
                mModel.get(TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET),
                MathUtils.EPSILON);
    }
}
