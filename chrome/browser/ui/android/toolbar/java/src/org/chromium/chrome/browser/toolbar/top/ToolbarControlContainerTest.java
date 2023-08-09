// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
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

/** Unit tests for {@link ToolbarControlContainer}. */
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
    @Mock
    private FullscreenManager mFullscreenManager;

    private final Supplier<Tab> mTabSupplier = () -> mTab;
    private final ObservableSupplierImpl<Boolean> mCompositorInMotionSupplier =
            new ObservableSupplierImpl<>();
    private final BrowserStateBrowserControlsVisibilityDelegate
            mBrowserStateBrowserControlsVisibilityDelegate =
                    new BrowserStateBrowserControlsVisibilityDelegate(
                            new ObservableSupplierImpl<>(false));
    private final AtomicInteger mOnResourceRequestedCount = new AtomicInteger();

    private boolean mIsVisible;
    private final BooleanSupplier mIsVisibleSupplier = () -> mIsVisible;

    private boolean mHasTestConstraintsOverride;
    private final ObservableSupplierImpl<Integer> mConstraintsSupplier =
            new ObservableSupplierImpl<>();
    private final OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();

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
                mIsVisibleSupplier, mLayoutStateProviderSupplier, mFullscreenManager);
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
        assertNotEquals(inMotion, mCompositorInMotionSupplier.get().booleanValue());
        int requestCount = mOnResourceRequestedCount.get();
        mCompositorInMotionSupplier.set(inMotion);
        ShadowLooper.idleMainLooper();
        int expectedCount = requestCount + (expectResourceRequested ? 1 : 0);
        assertEquals(expectedCount, mOnResourceRequestedCount.get());
    }

    private void setConstraintsOverride(Integer value) {
        mHasTestConstraintsOverride = true;
        mConstraintsSupplier.set(value);
    }

    @Before
    public void before() {
        mJniMocker.mock(ResourceFactoryJni.TEST_HOOKS, mResourceFactoryJni);
        UmaRecorderHolder.resetForTesting();
        when(mToolbarContainer.getWidth()).thenReturn(1);
        when(mToolbarContainer.getHeight()).thenReturn(1);
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

        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason"));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason"));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.SnapshotDifference"));

        assertFalse(adapter.isDirty());
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.TOOLBAR_OR_RESULT_NULL));

        initAdapter(adapter);
        assertFalse(adapter.isDirty());
        assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.TOOLBAR_OR_RESULT_NULL));

        when(mToolbar.isReadyForTextureCapture()).thenReturn(CaptureReadinessResult.unknown(true));
        assertTrue(adapter.isDirty());
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarBlockCaptureReason.UNKNOWN));

        adapter.triggerBitmapCapture();
        assertFalse(adapter.isDirty());
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.VIEW_NOT_DIRTY));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.SnapshotDifference"));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarBlockCaptureReason.UNKNOWN));

        adapter.forceInvalidate();
        assertTrue(adapter.isDirty());
        assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarBlockCaptureReason.UNKNOWN));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.SnapshotDifference"));
    }

    @Test
    public void testIsDirty_BlockedReason() {
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();
        when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.notReady(
                        TopToolbarBlockCaptureReason.SNAPSHOT_SAME));
        assertFalse(adapter.isDirty());
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.SNAPSHOT_SAME));
        assertTrue(adapter.getDirtyRect().isEmpty());
    }

    @Test
    public void testIsDirty_AllowForced() {
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();
        when(mToolbar.isReadyForTextureCapture()).thenReturn(CaptureReadinessResult.readyForced());
        assertTrue(adapter.isDirty());
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarAllowCaptureReason.FORCE_CAPTURE));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.SnapshotDifference"));
    }

    @Test
    public void testIsDirty_AllowSnapshotReason() {
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();
        when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        assertTrue(adapter.isDirty());
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.SnapshotDifference",
                        ToolbarSnapshotDifference.URL_TEXT));
    }

    @Test
    public void testIsDirty_ConstraintsSupplier() {
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();

        when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        when(mTab.isNativePage()).thenReturn(false);
        setConstraintsOverride(null);

        assertFalse(adapter.isDirty());
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.BROWSER_CONTROLS_LOCKED));
        assertEquals(0, mOnResourceRequestedCount.get());

        // SHOWN should be treated as still locked.
        setConstraintsOverride(BrowserControlsState.SHOWN);
        assertEquals(0, mOnResourceRequestedCount.get());

        // BOTH should cause a new onResourceRequested call.
        setConstraintsOverride(BrowserControlsState.BOTH);
        ShadowLooper.idleMainLooper();
        assertEquals(1, mOnResourceRequestedCount.get());

        // The constraints should no longer block isDirty/captures.
        assertTrue(adapter.isDirty());

        // Shouldn't be an observer subscribed now, changes shouldn't call onResourceRequested.
        setConstraintsOverride(BrowserControlsState.SHOWN);
        setConstraintsOverride(BrowserControlsState.BOTH);
        assertEquals(1, mOnResourceRequestedCount.get());
    }

    @Test
    public void testIsDirty_InMotion() {
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();

        when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        when(mTab.isNativePage()).thenReturn(false);
        mIsVisible = false;
        changeInMotion(/*inMotion*/ true, /*expectResourceRequested*/ false);

        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION));
        assertFalse(adapter.isDirty());
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION));
        assertFalse(didAdapterLockControls());

        changeInMotion(/*inMotion*/ false, /*expectResourceRequested*/ true);
    }

    @Test
    public void testIsDirty_InMotion2() {
        makeAndInitAdapter();
        // Unfortunately this gets emitted once during initialization and we cannot easily reset.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting("Android.TopToolbar.InMotionStage",
                        ToolbarInMotionStage.SUPPRESSION_ENABLED));
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting("Android.TopToolbar.InMotionStage",
                        ToolbarInMotionStage.READINESS_CHECKED));

        when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        when(mTab.isNativePage()).thenReturn(false);
        mIsVisible = true;

        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION));
        changeInMotion(/*inMotion*/ true, /*expectResourceRequested*/ false);
        assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting("Android.TopToolbar.InMotionStage",
                        ToolbarInMotionStage.SUPPRESSION_ENABLED));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting("Android.TopToolbar.InMotionStage",
                        ToolbarInMotionStage.READINESS_CHECKED));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION));
        assertTrue(didAdapterLockControls());

        changeInMotion(/*inMotion*/ false, /*expectResourceRequested*/ true);
        assertEquals(3,
                RecordHistogram.getHistogramValueCountForTesting("Android.TopToolbar.InMotionStage",
                        ToolbarInMotionStage.SUPPRESSION_ENABLED));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting("Android.TopToolbar.InMotionStage",
                        ToolbarInMotionStage.READINESS_CHECKED));
        assertFalse(didAdapterLockControls());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.RECORD_SUPPRESSION_METRICS)
    public void testIsDirty_InMotion2_NoMetrics() {
        assertFalse(ToolbarFeatures.shouldRecordSuppressionMetrics());
        makeAndInitAdapter();

        when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        when(mTab.isNativePage()).thenReturn(false);
        mIsVisible = true;

        changeInMotion(/*inMotion*/ true, /*expectResourceRequested*/ false);
        assertTrue(didAdapterLockControls());
        changeInMotion(/*inMotion*/ false, /*expectResourceRequested*/ true);
        assertFalse(didAdapterLockControls());

        assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting("Android.TopToolbar.InMotion"));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.InMotionStage"));
    }

    @Test
    public void testIsDirty_ConstraintsIgnoredOnNativePage() {
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();
        when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        when(mTab.isNativePage()).thenReturn(true);
        setConstraintsOverride(BrowserControlsState.SHOWN);

        assertTrue(adapter.isDirty());
    }

    @Test
    public void testInMotion_viewNotVisible() {
        makeAndInitAdapter();
        when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        mIsVisible = false;

        changeInMotion(true, false);
    }

    @Test
    public void testIsDirty_InMotionAndToolbarSwipe() {
        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();
        changeInMotion(true, false);
        when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        adapter.forceInvalidate();
        when(mLayoutStateProvider.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        // The supplier posts the notification so idle to let it through.
        ShadowLooper.idleMainLooper();

        assertFalse(adapter.isDirty());

        // TOOLBAR_SWIPE should bypass the in motion check and return dirty.
        when(mLayoutStateProvider.getActiveLayoutType()).thenReturn(LayoutType.TOOLBAR_SWIPE);

        assertTrue(adapter.isDirty());
    }

    @Test
    public void testIsDirty_Fullscreen() {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES, true);
        testValues.addFieldTrialParamOverride(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES,
                ToolbarFeatures.BLOCK_FOR_FULLSCREEN, "true");
        FeatureList.setTestValues(testValues);

        when(mFullscreenManager.getPersistentFullscreenMode()).thenReturn(true);

        ToolbarViewResourceAdapter adapter = makeAndInitAdapter();

        when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));

        assertFalse(adapter.isDirty());
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.FULLSCREEN));

        when(mFullscreenManager.getPersistentFullscreenMode()).thenReturn(false);
        assertTrue(adapter.isDirty());
    }
}
