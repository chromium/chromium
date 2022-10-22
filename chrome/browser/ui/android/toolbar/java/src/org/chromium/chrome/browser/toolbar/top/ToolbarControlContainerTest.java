// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.supplier.BooleanSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.JniMocker;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarAllowCaptureReason;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer.ToolbarViewResourceAdapter;
import org.chromium.chrome.browser.toolbar.top.ToolbarSnapshotState.ToolbarSnapshotDifference;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.Features.JUnitProcessor;

import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for ToolbarControlContainer. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {HomeButtonCoordinatorTest.ShadowChromeFeatureList.class})
@EnableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
public class ToolbarControlContainerTest {
    @Rule
    public MockitoRule rule = MockitoJUnit.rule();
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public TestRule mFeaturesProcessor = new JUnitProcessor();

    @Mock
    private ResourceFactory.Natives mResourceFactoryJni;
    @Mock
    private View mToolbarContainer;
    @Mock
    private Toolbar mToolbar;
    @Mock
    private Tab mTab;

    private final Supplier<Tab> mTabSupplier = () -> mTab;
    private final ObservableSupplierImpl<Integer> mConstraintsSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mCompositorInMotionSupplier =
            new ObservableSupplierImpl<>();
    private final BrowserStateBrowserControlsVisibilityDelegate
            mBrowserStateBrowserControlsVisibilityDelegate =
                    new BrowserStateBrowserControlsVisibilityDelegate(initBooleanSupplier(false));

    private boolean mIsVisible;
    private final BooleanSupplier mIsVisibleSupplier = () -> mIsVisible;

    /**
     * Returns an initialized ObservableSupplier<Boolean>, otherwise not possible to init inline.
     */
    private static ObservableSupplier<Boolean> initBooleanSupplier(boolean value) {
        ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
        supplier.set(value);
        return supplier;
    }

    private void initAdapter(ToolbarViewResourceAdapter adapter) {
        adapter.setPostInitializationDependencies(mToolbar, mConstraintsSupplier, mTabSupplier,
                mCompositorInMotionSupplier, mBrowserStateBrowserControlsVisibilityDelegate,
                mIsVisibleSupplier);
        // The adapter may observe some of these already, which will post events.
        ShadowLooper.idleMainLooper();
    }

    private boolean areControlsLocked() {
        return mBrowserStateBrowserControlsVisibilityDelegate.get() == BrowserControlsState.SHOWN;
    }

    @Before
    public void before() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(ResourceFactoryJni.TEST_HOOKS, mResourceFactoryJni);
        UmaRecorderHolder.resetForTesting();
        Mockito.when(mToolbarContainer.getWidth()).thenReturn(1);
        Mockito.when(mToolbarContainer.getHeight()).thenReturn(1);
        mConstraintsSupplier.set(BrowserControlsState.BOTH);
        mCompositorInMotionSupplier.set(false);
    }

    @Test
    @DisabledTest(message = "Temporarily disabled due to https://crbug.com/1344612")
    public void testIsDirty() {
        ToolbarViewResourceAdapter adapter =
                new ToolbarViewResourceAdapter(mToolbarContainer, false);
        adapter.setOnResourceReadyCallback((resource) -> {});

        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason"));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason"));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.SnapshotDifference"));

        Assert.assertFalse(adapter.isDirty());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.TOOLBAR_OR_RESULT_NULL));

        initAdapter(adapter);
        Assert.assertFalse(adapter.isDirty());
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.TOOLBAR_OR_RESULT_NULL));

        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.unknown(true));
        Assert.assertTrue(adapter.isDirty());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarBlockCaptureReason.UNKNOWN));

        adapter.triggerBitmapCapture();
        Assert.assertFalse(adapter.isDirty());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.VIEW_NOT_DIRTY));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.SnapshotDifference"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarBlockCaptureReason.UNKNOWN));

        adapter.forceInvalidate();
        Assert.assertTrue(adapter.isDirty());
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarBlockCaptureReason.UNKNOWN));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.SnapshotDifference"));
    }

    @Test
    public void testIsDirty_BlockedReason() {
        ToolbarViewResourceAdapter adapter =
                new ToolbarViewResourceAdapter(mToolbarContainer, false);
        initAdapter(adapter);
        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.notReady(
                        TopToolbarBlockCaptureReason.SNAPSHOT_SAME));
        Assert.assertFalse(adapter.isDirty());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.SNAPSHOT_SAME));
        Assert.assertTrue(adapter.getDirtyRect().isEmpty());
    }

    @Test
    public void testIsDirty_AllowForced() {
        ToolbarViewResourceAdapter adapter =
                new ToolbarViewResourceAdapter(mToolbarContainer, false);
        initAdapter(adapter);
        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyForced());
        Assert.assertTrue(adapter.isDirty());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarAllowCaptureReason.FORCE_CAPTURE));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.SnapshotDifference"));
    }

    @Test
    public void testIsDirty_AllowSnapshotReason() {
        ToolbarViewResourceAdapter adapter =
                new ToolbarViewResourceAdapter(mToolbarContainer, false);
        initAdapter(adapter);
        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        Assert.assertTrue(adapter.isDirty());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.SnapshotDifference",
                        ToolbarSnapshotDifference.URL_TEXT));
    }

    @Test
    public void testIsDirty_ConstraintsSupplier() {
        AtomicInteger onResourceRequestedCount = new AtomicInteger();
        ToolbarViewResourceAdapter adapter =
                new ToolbarViewResourceAdapter(mToolbarContainer, false) {
                    @Override
                    public void onResourceRequested() {
                        // No-op normal functionality and just count calls instead.
                        onResourceRequestedCount.getAndIncrement();
                    }
                };
        initAdapter(adapter);
        onResourceRequestedCount.set(0);

        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        Mockito.when(mTab.isNativePage()).thenReturn(false);
        mConstraintsSupplier.set(null);

        Assert.assertFalse(adapter.isDirty());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.BROWSER_CONTROLS_LOCKED));
        Assert.assertEquals(0, onResourceRequestedCount.get());

        // SHOWN should be treated as still locked.
        mConstraintsSupplier.set(BrowserControlsState.SHOWN);
        Assert.assertEquals(0, onResourceRequestedCount.get());

        // BOTH should cause a new onResourceRequested call.
        mConstraintsSupplier.set(BrowserControlsState.BOTH);
        ShadowLooper.idleMainLooper();
        Assert.assertEquals(1, onResourceRequestedCount.get());

        // The constraints should no longer block isDirty/captures.
        Assert.assertTrue(adapter.isDirty());

        // Shouldn't be an observer subscribed now, changes shouldn't call onResourceRequested.
        mConstraintsSupplier.set(BrowserControlsState.SHOWN);
        mConstraintsSupplier.set(BrowserControlsState.BOTH);
        Assert.assertEquals(1, onResourceRequestedCount.get());
    }

    @Test
    public void testIsDirty_InMotion() {
        AtomicInteger onResourceRequestedCount = new AtomicInteger();

        ToolbarViewResourceAdapter adapter =
                new ToolbarViewResourceAdapter(mToolbarContainer, false) {
                    @Override
                    public void onResourceRequested() {
                        // No-op normal functionality and just count calls instead.
                        onResourceRequestedCount.getAndIncrement();
                    }
                };
        initAdapter(adapter);
        onResourceRequestedCount.set(0);

        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        Mockito.when(mTab.isNativePage()).thenReturn(false);
        mIsVisible = false;
        mCompositorInMotionSupplier.set(true);

        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION));
        Assert.assertFalse(adapter.isDirty());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION));
        Assert.assertFalse(areControlsLocked());
        ShadowLooper.idleMainLooper();
        Assert.assertEquals(0, onResourceRequestedCount.get());

        mCompositorInMotionSupplier.set(false);
        ShadowLooper.idleMainLooper();
        Assert.assertEquals(1, onResourceRequestedCount.get());
    }

    @Test
    public void testIsDirty_InMotion2() {
        AtomicInteger onResourceRequestedCount = new AtomicInteger();

        ToolbarViewResourceAdapter adapter =
                new ToolbarViewResourceAdapter(mToolbarContainer, false) {
                    @Override
                    public void onResourceRequested() {
                        // No-op normal functionality and just count calls instead.
                        onResourceRequestedCount.getAndIncrement();
                    }
                };
        initAdapter(adapter);
        onResourceRequestedCount.set(0);

        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        Mockito.when(mTab.isNativePage()).thenReturn(false);
        mIsVisible = true;

        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION));
        mCompositorInMotionSupplier.set(true);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION));
        Assert.assertTrue(areControlsLocked());

        mCompositorInMotionSupplier.set(false);
        ShadowLooper.idleMainLooper();
        Assert.assertFalse(areControlsLocked());
        Assert.assertEquals(1, onResourceRequestedCount.get());
    }

    @Test
    public void testIsDirty_ConstraintsIgnoredOnNativePage() {
        ToolbarViewResourceAdapter adapter =
                new ToolbarViewResourceAdapter(mToolbarContainer, false);
        initAdapter(adapter);
        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        Mockito.when(mTab.isNativePage()).thenReturn(true);
        mConstraintsSupplier.set(BrowserControlsState.SHOWN);

        Assert.assertTrue(adapter.isDirty());
    }
}
