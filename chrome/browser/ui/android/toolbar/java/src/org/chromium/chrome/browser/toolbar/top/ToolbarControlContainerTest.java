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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarAllowCaptureReason;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer.ToolbarViewResourceAdapter;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer.ToolbarViewResourceAdapter.ToolbarInMotionStage;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.Features.JUnitProcessor;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.BooleanSupplier;

/** Unit tests for ToolbarControlContainer. */
@RunWith(BaseRobolectricTestRunner.class)
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
    @Mock
    private LayoutStateProvider mLayoutStateProvider;

    private final Supplier<Tab> mTabSupplier = () -> mTab;
    private final ObservableSupplierImpl<Boolean> mCompositorInMotionSupplier =
            new ObservableSupplierImpl<>();
    private final BrowserStateBrowserControlsVisibilityDelegate
            mBrowserStateBrowserControlsVisibilityDelegate =
                    new BrowserStateBrowserControlsVisibilityDelegate(initBooleanSupplier(false));
    private final AtomicInteger mOnResourceRequestedCount = new AtomicInteger();

    private boolean mIsVisible;
    private final BooleanSupplier mIsVisibleSupplier = () -> mIsVisible;

    private boolean mHasTestConstraintsOverride;
    private final ObservableSupplierImpl<Integer> mConstraintsSupplier =
            new ObservableSupplierImpl<>();
    private final OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();

    /**
     * Returns an initialized ObservableSupplier<Boolean>, otherwise not possible to init inline.
     */
    private static ObservableSupplier<Boolean> initBooleanSupplier(boolean value) {
        ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
        supplier.set(value);
        return supplier;
    }

    private ToolbarViewResourceAdapter makeAdapter() {
        return new ToolbarViewResourceAdapter(mToolbarContainer, false) {
            @Override
            public void onResourceRequested() {
                // No-op normal functionality and just count calls instead.
                mOnResourceRequestedCount.getAndIncrement();
            }
        };
    }

    private void initAdapter(ToolbarViewResourceAdapter adapter) {
        adapter.setPostInitializationDependencies(mToolbar, mConstraintsSupplier, mTabSupplier,
                mCompositorInMotionSupplier, mBrowserStateBrowserControlsVisibilityDelegate,
                mIsVisibleSupplier, mLayoutStateProviderSupplier);
        // The adapter may observe some of these already, which will post events.
        ShadowLooper.idleMainLooper();
        // The initial addObserver triggers an event that we don't care about. Reset count.
        mOnResourceRequestedCount.set(0);
    }

    private ToolbarViewResourceAdapter makeAndInitAdapter() {
        ToolbarViewResourceAdapter adapter = makeAdapter();
        initAdapter(adapter);
        return adapter;
    }

    private boolean didAdapterLockControls() {
        return mBrowserStateBrowserControlsVisibilityDelegate.get() == BrowserControlsState.SHOWN;
    }

    private void changeInMotion(boolean inMotion, boolean expectResourceRequested) {
        Assert.assertFalse(inMotion == mCompositorInMotionSupplier.get());
        int requestCount = mOnResourceRequestedCount.get();
        mCompositorInMotionSupplier.set(inMotion);
        ShadowLooper.idleMainLooper();
        int expectedCount = requestCount + (expectResourceRequested ? 1 : 0);
        Assert.assertEquals(expectedCount, mOnResourceRequestedCount.get());
    }

    private void setConstraintsOverride(Integer value) {
        mHasTestConstraintsOverride = true;
        mConstraintsSupplier.set(value);
    }

    @Before
    public void before() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(ResourceFactoryJni.TEST_HOOKS, mResourceFactoryJni);
        UmaRecorderHolder.resetForTesting();
        Mockito.when(mToolbarContainer.getWidth()).thenReturn(1);
        Mockito.when(mToolbarContainer.getHeight()).thenReturn(1);
        mBrowserStateBrowserControlsVisibilityDelegate.set(BrowserControlsState.BOTH);
        mCompositorInMotionSupplier.set(false);
        mBrowserStateBrowserControlsVisibilityDelegate.addObserver(result -> {
            if (!mHasTestConstraintsOverride) {
                mConstraintsSupplier.set(result);
            }
        });
    }

    @Test
    public void testIsDirty() {
        ToolbarViewResourceAdapter adapter = makeAdapter();
        adapter.addOnResourceReadyCallback((resource) -> {});

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
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();
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
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();
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
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();
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
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();

        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        Mockito.when(mTab.isNativePage()).thenReturn(false);
        setConstraintsOverride(null);

        Assert.assertFalse(adapter.isDirty());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.BROWSER_CONTROLS_LOCKED));
        Assert.assertEquals(0, mOnResourceRequestedCount.get());

        // SHOWN should be treated as still locked.
        setConstraintsOverride(BrowserControlsState.SHOWN);
        Assert.assertEquals(0, mOnResourceRequestedCount.get());

        // BOTH should cause a new onResourceRequested call.
        setConstraintsOverride(BrowserControlsState.BOTH);
        ShadowLooper.idleMainLooper();
        Assert.assertEquals(1, mOnResourceRequestedCount.get());

        // The constraints should no longer block isDirty/captures.
        Assert.assertTrue(adapter.isDirty());

        // Shouldn't be an observer subscribed now, changes shouldn't call onResourceRequested.
        setConstraintsOverride(BrowserControlsState.SHOWN);
        setConstraintsOverride(BrowserControlsState.BOTH);
        Assert.assertEquals(1, mOnResourceRequestedCount.get());
    }

    @Test
    public void testIsDirty_InMotion() {
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();

        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        Mockito.when(mTab.isNativePage()).thenReturn(false);
        mIsVisible = false;
        changeInMotion(/*inMotion*/ true, /*expectResourceRequested*/ false);

        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION));
        Assert.assertFalse(adapter.isDirty());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION));
        Assert.assertFalse(didAdapterLockControls());

        changeInMotion(/*inMotion*/ false, /*expectResourceRequested*/ true);
    }

    @Test
    public void testIsDirty_InMotion2() {
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();
        // Unfortunately this gets emitted once during initialization and we cannot easily reset.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting("Android.TopToolbar.InMotionStage",
                        ToolbarInMotionStage.SUPPRESSION_ENABLED));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting("Android.TopToolbar.InMotionStage",
                        ToolbarInMotionStage.READINESS_CHECKED));

        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        Mockito.when(mTab.isNativePage()).thenReturn(false);
        mIsVisible = true;

        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION));
        changeInMotion(/*inMotion*/ true, /*expectResourceRequested*/ false);
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting("Android.TopToolbar.InMotionStage",
                        ToolbarInMotionStage.SUPPRESSION_ENABLED));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting("Android.TopToolbar.InMotionStage",
                        ToolbarInMotionStage.READINESS_CHECKED));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION));
        Assert.assertTrue(didAdapterLockControls());

        changeInMotion(/*inMotion*/ false, /*expectResourceRequested*/ true);
        Assert.assertEquals(3,
                RecordHistogram.getHistogramValueCountForTesting("Android.TopToolbar.InMotionStage",
                        ToolbarInMotionStage.SUPPRESSION_ENABLED));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting("Android.TopToolbar.InMotionStage",
                        ToolbarInMotionStage.READINESS_CHECKED));
        Assert.assertFalse(didAdapterLockControls());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.RECORD_SUPPRESSION_METRICS)
    public void testIsDirty_InMotion2_NoMetrics() {
        Assert.assertFalse(ToolbarFeatures.shouldRecordSuppressionMetrics());
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();

        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        Mockito.when(mTab.isNativePage()).thenReturn(false);
        mIsVisible = true;

        changeInMotion(/*inMotion*/ true, /*expectResourceRequested*/ false);
        Assert.assertTrue(didAdapterLockControls());
        changeInMotion(/*inMotion*/ false, /*expectResourceRequested*/ true);
        Assert.assertFalse(didAdapterLockControls());

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting("Android.TopToolbar.InMotion"));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.InMotionStage"));
    }

    @Test
    public void testIsDirty_ConstraintsIgnoredOnNativePage() {
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();
        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        Mockito.when(mTab.isNativePage()).thenReturn(true);
        setConstraintsOverride(BrowserControlsState.SHOWN);

        Assert.assertTrue(adapter.isDirty());
    }

    @Test
    public void testInMotion_viewNotVisible() {
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();
        Mockito.doReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                                 ToolbarSnapshotDifference.URL_TEXT))
                .when(mToolbar)
                .isReadyForTextureCapture();
        mIsVisible = false;

        changeInMotion(true, false);
    }

    @Test
    public void testIsDirty_InMotionAndToolbarSwipe() {
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();
        changeInMotion(true, false);
        Mockito.doReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                                 ToolbarSnapshotDifference.URL_TEXT))
                .when(mToolbar)
                .isReadyForTextureCapture();
        adapter.forceInvalidate();
        Mockito.doReturn(LayoutType.BROWSING).when(mLayoutStateProvider).getActiveLayoutType();
        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        // The supplier posts the notification so idle to let it through.
        ShadowLooper.idleMainLooper();

        Assert.assertFalse(adapter.isDirty());

        // TOOLBAR_SWIPE should bypass the in motion check and return dirty.
        Mockito.doReturn(LayoutType.TOOLBAR_SWIPE).when(mLayoutStateProvider).getActiveLayoutType();

        Assert.assertTrue(adapter.isDirty());
    }
}
