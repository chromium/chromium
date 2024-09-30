// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
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
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.BooleanSupplier;

/** Unit tests for {@link ToolbarControlContainer}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
public class ToolbarControlContainerTest {
    private static final String BLOCK_NAME = "Android.TopToolbar.BlockCaptureReason";
    private static final String ALLOW_NAME = "Android.TopToolbar.AllowCaptureReason";
    private static final String DIFFERENCE_NAME = "Android.TopToolbar.SnapshotDifference";
    private static final String MOTION_STAGE_NAME = "Android.TopToolbar.InMotionStage";

    @Rule public MockitoRule rule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private ResourceFactory.Natives mResourceFactoryJni;
    @Mock private View mToolbarContainer;
    @Mock private View mToolbarHairline;
    @Mock private Toolbar mToolbar;
    @Mock private Tab mTab;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private FullscreenManager mFullscreenManager;

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

    private ToolbarViewResourceAdapter mAdapter;

    private void makeAdapter() {
        mAdapter =
                new ToolbarViewResourceAdapter(mToolbarContainer) {
                    @Override
                    public void onResourceRequested() {
                        // No-op normal functionality and just count calls instead.
                        mOnResourceRequestedCount.getAndIncrement();
                    }
                };
    }

    private void initAdapter() {
        mAdapter.setPostInitializationDependencies(
                mToolbar,
                mConstraintsSupplier,
                mTabSupplier,
                mCompositorInMotionSupplier,
                mBrowserStateBrowserControlsVisibilityDelegate,
                mIsVisibleSupplier,
                mLayoutStateProviderSupplier,
                mFullscreenManager);
        // The adapter may observe some of these already, which will post events.
        ShadowLooper.idleMainLooper();
        // The initial addObserver triggers an event that we don't care about. Reset count.
        mOnResourceRequestedCount.set(0);
    }

    private void makeAndInitAdapter() {
        makeAdapter();
        initAdapter();
    }

    private boolean didAdapterLockControls() {
        return mBrowserStateBrowserControlsVisibilityDelegate.get() == BrowserControlsState.SHOWN;
    }

    private void verifyRequestsOnInMotionChange(boolean inMotion, boolean expectResourceRequested) {
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

    private void mockIsReadyDifference(@ToolbarSnapshotDifference int difference) {
        when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(difference));
    }

    private void verifyIsDirtyWasBlocked(@TopToolbarBlockCaptureReason int reason) {
        verifyIsDirtyHelper(false, reason, null, null);
    }

    private void verifyIsDirtyWasAllowed(@TopToolbarAllowCaptureReason int reason) {
        verifyIsDirtyHelper(true, null, reason, null);
    }

    private void verifyIsDirtyWasAllowedForSnapshot(@ToolbarSnapshotDifference int difference) {
        verifyIsDirtyHelper(
                true, null, TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE, difference);
    }

    private void verifyIsDirtyHelper(
            boolean isDirty, Integer blockValue, Integer allowValue, Integer differenceValue) {
        HistogramWatcher.Builder builder = HistogramWatcher.newBuilder();
        expectIntRecord(builder, BLOCK_NAME, blockValue);
        expectIntRecord(builder, ALLOW_NAME, allowValue);
        expectIntRecord(builder, DIFFERENCE_NAME, differenceValue);
        HistogramWatcher histogramWatcher = builder.build();
        assertEquals(isDirty, mAdapter.isDirty());
        histogramWatcher.assertExpected();
    }

    private void expectIntRecord(HistogramWatcher.Builder builder, String name, Integer value) {
        if (value == null) {
            builder.expectNoRecords(name);
        } else {
            builder.expectIntRecord(name, value.intValue());
        }
    }

    @Before
    public void before() {
        mJniMocker.mock(ResourceFactoryJni.TEST_HOOKS, mResourceFactoryJni);
        when(mToolbarContainer.getWidth()).thenReturn(1);
        when(mToolbarContainer.getHeight()).thenReturn(1);
        when(mToolbarContainer.findViewById(anyInt())).thenReturn(mToolbarHairline);
        when(mToolbarHairline.getHeight()).thenReturn(1);
        mBrowserStateBrowserControlsVisibilityDelegate.set(BrowserControlsState.BOTH);
        mCompositorInMotionSupplier.set(false);
        mBrowserStateBrowserControlsVisibilityDelegate.addObserver(
                result -> {
                    if (!mHasTestConstraintsOverride) {
                        mConstraintsSupplier.set(result);
                    }
                });
    }

    @Test
    public void testIsDirty() {
        makeAdapter();
        mAdapter.addOnResourceReadyCallback((resource) -> {});
        verifyIsDirtyWasBlocked(TopToolbarBlockCaptureReason.TOOLBAR_OR_RESULT_NULL);

        initAdapter();
        verifyIsDirtyWasBlocked(TopToolbarBlockCaptureReason.TOOLBAR_OR_RESULT_NULL);

        when(mToolbar.isReadyForTextureCapture()).thenReturn(CaptureReadinessResult.unknown(true));
        verifyIsDirtyWasAllowed(TopToolbarAllowCaptureReason.UNKNOWN);

        mAdapter.triggerBitmapCapture();
        verifyIsDirtyWasBlocked(TopToolbarBlockCaptureReason.VIEW_NOT_DIRTY);

        mAdapter.forceInvalidate();
        verifyIsDirtyWasAllowed(TopToolbarAllowCaptureReason.UNKNOWN);
    }

    @Test
    public void testIsDirty_BlockedReason() {
        final @TopToolbarBlockCaptureReason int reason = TopToolbarBlockCaptureReason.SNAPSHOT_SAME;
        makeAndInitAdapter();
        when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.notReady(reason));

        verifyIsDirtyWasBlocked(reason);
        assertTrue(mAdapter.getDirtyRect().isEmpty());
    }

    @Test
    public void testIsDirty_AllowForced() {
        makeAndInitAdapter();
        when(mToolbar.isReadyForTextureCapture()).thenReturn(CaptureReadinessResult.readyForced());
        verifyIsDirtyWasAllowed(TopToolbarAllowCaptureReason.FORCE_CAPTURE);
    }

    @Test
    public void testIsDirty_AllowSnapshotReason() {
        final @ToolbarSnapshotDifference int difference = ToolbarSnapshotDifference.URL_TEXT;
        makeAndInitAdapter();
        mockIsReadyDifference(ToolbarSnapshotDifference.URL_TEXT);

        verifyIsDirtyWasAllowedForSnapshot(difference);
    }

    @Test
    public void testIsDirty_ConstraintsSupplier() {
        makeAndInitAdapter();

        final @ToolbarSnapshotDifference int difference = ToolbarSnapshotDifference.URL_TEXT;
        mockIsReadyDifference(difference);
        when(mTab.isNativePage()).thenReturn(false);
        setConstraintsOverride(null);

        verifyIsDirtyWasBlocked(TopToolbarBlockCaptureReason.BROWSER_CONTROLS_LOCKED);
        assertEquals(0, mOnResourceRequestedCount.get());

        // SHOWN should be treated as still locked.
        setConstraintsOverride(BrowserControlsState.SHOWN);
        assertEquals(0, mOnResourceRequestedCount.get());

        // BOTH should cause a new onResourceRequested call.
        setConstraintsOverride(BrowserControlsState.BOTH);
        ShadowLooper.idleMainLooper();
        assertEquals(1, mOnResourceRequestedCount.get());

        // The constraints should no longer block isDirty/captures.
        verifyIsDirtyWasAllowedForSnapshot(difference);

        // Shouldn't be an observer subscribed now, changes shouldn't call onResourceRequested.
        setConstraintsOverride(BrowserControlsState.SHOWN);
        setConstraintsOverride(BrowserControlsState.BOTH);
        assertEquals(1, mOnResourceRequestedCount.get());
    }

    @Test
    public void testIsDirty_InMotion() {
        makeAndInitAdapter();
        mockIsReadyDifference(ToolbarSnapshotDifference.URL_TEXT);
        when(mTab.isNativePage()).thenReturn(false);
        mIsVisible = false;

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(BLOCK_NAME).build();
        verifyRequestsOnInMotionChange(/* inMotion= */ true, /* expectResourceRequested= */ false);
        histogramWatcher.assertExpected();

        verifyIsDirtyWasBlocked(TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION);
        assertFalse(didAdapterLockControls());

        verifyRequestsOnInMotionChange(/* inMotion= */ false, /* expectResourceRequested= */ true);
    }

    @Test
    public void testIsDirty_InMotion2() {
        makeAndInitAdapter();
        mockIsReadyDifference(ToolbarSnapshotDifference.URL_TEXT);
        when(mTab.isNativePage()).thenReturn(false);
        mIsVisible = true;

        try (HistogramWatcher ignored =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MOTION_STAGE_NAME, ToolbarInMotionStage.SUPPRESSION_ENABLED)
                        .expectIntRecord(MOTION_STAGE_NAME, ToolbarInMotionStage.READINESS_CHECKED)
                        .expectIntRecord(
                                BLOCK_NAME, TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION)
                        .build()) {
            verifyRequestsOnInMotionChange(
                    /* inMotion= */ true, /* expectResourceRequested= */ false);
        }
        assertTrue(didAdapterLockControls());

        try (HistogramWatcher ignored =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MOTION_STAGE_NAME, ToolbarInMotionStage.SUPPRESSION_ENABLED)
                        .expectNoRecords(BLOCK_NAME)
                        .expectNoRecords(ALLOW_NAME)
                        .build()) {
            verifyRequestsOnInMotionChange(
                    /* inMotion= */ false, /* expectResourceRequested= */ true);
        }
        assertFalse(didAdapterLockControls());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.RECORD_SUPPRESSION_METRICS)
    public void testIsDirty_InMotion2_NoMetrics() {
        assertFalse(ToolbarFeatures.shouldRecordSuppressionMetrics());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.TopToolbar.InMotion")
                        .expectNoRecords("Android.TopToolbar.InMotionStage")
                        .build();
        makeAndInitAdapter();
        mockIsReadyDifference(ToolbarSnapshotDifference.URL_TEXT);
        when(mTab.isNativePage()).thenReturn(false);
        mIsVisible = true;

        verifyRequestsOnInMotionChange(/* inMotion= */ true, /* expectResourceRequested= */ false);
        assertTrue(didAdapterLockControls());
        verifyRequestsOnInMotionChange(/* inMotion= */ false, /* expectResourceRequested= */ true);
        assertFalse(didAdapterLockControls());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testIsDirty_ConstraintsIgnoredOnNativePage() {
        makeAndInitAdapter();
        final @ToolbarSnapshotDifference int difference = ToolbarSnapshotDifference.URL_TEXT;
        mockIsReadyDifference(difference);
        when(mTab.isNativePage()).thenReturn(true);
        setConstraintsOverride(BrowserControlsState.SHOWN);

        verifyIsDirtyWasAllowedForSnapshot(difference);
    }

    @Test
    public void testInMotion_viewNotVisible() {
        makeAndInitAdapter();
        mockIsReadyDifference(ToolbarSnapshotDifference.URL_TEXT);
        mIsVisible = false;

        verifyRequestsOnInMotionChange(true, false);
    }

    @Test
    public void testIsDirty_InMotionAndToolbarSwipe() {
        makeAndInitAdapter();
        verifyRequestsOnInMotionChange(true, false);
        mockIsReadyDifference(ToolbarSnapshotDifference.URL_TEXT);
        when(mLayoutStateProvider.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        // The supplier posts the notification so idle to let it through.
        ShadowLooper.idleMainLooper();

        verifyIsDirtyWasBlocked(TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION);

        // TOOLBAR_SWIPE should bypass the in motion check and return dirty.
        when(mLayoutStateProvider.getActiveLayoutType()).thenReturn(LayoutType.TOOLBAR_SWIPE);

        verifyIsDirtyWasAllowedForSnapshot(ToolbarSnapshotDifference.URL_TEXT);
    }

    @Test
    public void testIsDirty_Fullscreen() {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES, true);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.sShouldBlockCapturesForFullscreenParam, "true");
        FeatureList.setTestValues(testValues);

        final @ToolbarSnapshotDifference int difference = ToolbarSnapshotDifference.URL_TEXT;
        when(mFullscreenManager.getPersistentFullscreenMode()).thenReturn(true);
        makeAndInitAdapter();
        mockIsReadyDifference(difference);

        verifyIsDirtyWasBlocked(TopToolbarBlockCaptureReason.FULLSCREEN);

        when(mFullscreenManager.getPersistentFullscreenMode()).thenReturn(false);
        verifyIsDirtyWasAllowedForSnapshot(difference);
    }

    @Test
    public void testTempDrawableWithAppHeaderState() {
        TestActivity activity = Robolectric.buildActivity(TestActivity.class).get();
        ToolbarControlContainer controlContainer = new ToolbarControlContainer(activity, null);
        // This is needed for the control container to read the height of the toolbar.
        controlContainer.setToolbarForTesting(mToolbar);

        // Set app header with 10px padding on left, 20px on right, and 50px height.
        doReturn(50).when(mToolbar).getTabStripHeight();
        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 50), new Rect(10, 0, 80, 50), true);
        controlContainer.onAppHeaderStateChanged(appHeaderState);
        assertNotNull(
                "Control container background is null after app header state change.",
                controlContainer.getBackground());

        LayerDrawable background = (LayerDrawable) controlContainer.getBackground();
        final int tabDrawableIndex = 1;
        assertEquals(
                "Left padding for tab drawable is wrong.",
                10,
                background.getLayerInsetLeft(tabDrawableIndex));
        assertEquals(
                "Right padding for tab drawable is wrong.",
                20,
                background.getLayerInsetRight(tabDrawableIndex));

        controlContainer.onAppHeaderStateChanged(new AppHeaderState());
        background = (LayerDrawable) controlContainer.getBackground();
        assertEquals(
                "Left padding for tab drawable is wrong.",
                0,
                background.getLayerInsetLeft(tabDrawableIndex));
        assertEquals(
                "Right padding for tab drawable is wrong.",
                0,
                background.getLayerInsetRight(tabDrawableIndex));

        activity.finish();
    }

    @Test
    public void testTempDrawableAfterCompositorInitialized() {
        TestActivity activity = Robolectric.buildActivity(TestActivity.class).get();
        ToolbarControlContainer controlContainer = new ToolbarControlContainer(activity, null);
        // This is needed for the control container to read the height of the toolbar.
        controlContainer.setToolbarForTesting(mToolbar);
        controlContainer.setCompositorBackgroundInitialized();
        assertNull(
                "Control container background should be null after app header state change.",
                controlContainer.getBackground());

        // Set app header with 10px padding on left, 20px on right, and 50px height.
        doReturn(50).when(mToolbar).getTabStripHeight();
        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 50), new Rect(10, 0, 80, 50), true);
        controlContainer.onAppHeaderStateChanged(appHeaderState);
        assertNull(
                "Control container background should not respond to app header state anymore.",
                controlContainer.getBackground());

        activity.finish();
    }

    @Test
    public void testTempDrawableInUnfocusedDesktopWindow() {
        TestActivity activity = Robolectric.buildActivity(TestActivity.class).get();
        ToolbarControlContainer controlContainer = new ToolbarControlContainer(activity, null);
        // This is needed for the control container to read the height of the toolbar.
        controlContainer.setToolbarForTesting(mToolbar);

        // Assume that the app started in an unfocused desktop window.
        controlContainer.setAppInUnfocusedDesktopWindow(true);

        // Simulate invocation of app header state change at startup that sets the temp drawable.
        doReturn(50).when(mToolbar).getTabStripHeight();
        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 50), new Rect(10, 0, 80, 50), true);
        controlContainer.onAppHeaderStateChanged(appHeaderState);

        var backgroundLayerDrawable = (LayerDrawable) controlContainer.getBackground();
        var stripBackgroundColorDrawable = (ColorDrawable) backgroundLayerDrawable.getDrawable(0);
        assertEquals(
                "Tab strip background color drawable color is incorrect.",
                ChromeColors.getSurfaceColor(
                        controlContainer.getContext(), R.dimen.default_elevation_2),
                stripBackgroundColorDrawable.getColor());

        activity.finish();
    }
}
