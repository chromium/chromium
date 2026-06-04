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
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewTreeObserver;

import androidx.annotation.LayoutRes;
import androidx.coordinatorlayout.widget.CoordinatorLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinatorPhone;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.ToolbarHairlineView;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.back_button.BackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.forward_button.ForwardButtonCoordinator;
import org.chromium.chrome.browser.toolbar.home_button.HomeButtonCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator;
import org.chromium.chrome.browser.toolbar.reload_button.ReloadButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarAllowCaptureReason;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer.ToolbarViewResourceAdapter;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer.ToolbarViewResourceAdapter.ToolbarInMotionStage;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer.ToolbarViewResourceCoordinatorLayout;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/** Unit tests for {@link ToolbarControlContainer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ToolbarControlContainerTest {
    private static final String BLOCK_NAME = "Android.TopToolbar.BlockCaptureReason";
    private static final String ALLOW_NAME = "Android.TopToolbar.AllowCaptureReason";
    private static final String DIFFERENCE_NAME = "Android.TopToolbar.SnapshotDifference";
    private static final String MOTION_STAGE_NAME = "Android.TopToolbar.InMotionStage";

    @Rule public MockitoRule rule = MockitoJUnit.rule();

    @Mock private ResourceFactory.Natives mResourceFactoryJni;
    @Mock private View mToolbarContainer;
    @Mock private ViewGroup mToolbarView;
    @Mock private View mLocationBarView;
    @Mock private ToolbarHairlineView mToolbarHairline;
    @Mock private Toolbar mToolbar;
    @Mock private ToggleTabStackButtonCoordinator mTabSwitcherButtonCoordinator;
    @Mock private ToolbarProgressBar mProgressBar;
    @Mock private Tab mTab;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private TouchEventObserver mTouchEventObserver;
    @Mock private LocationBarCoordinator mLocationBarCoordinator;
    @Mock private LocationBarCoordinatorPhone mLocationBarCoordinatorPhone;
    @Mock private MenuButtonCoordinator mMenuButtonCoordinator;
    @Mock private ToolbarDataProvider mToolbarDataProvider;
    @Mock private ReloadButtonCoordinator mReloadButtonCoordinator;
    @Mock private BackButtonCoordinator mBackButtonCoordinator;
    @Mock private ForwardButtonCoordinator mForwardButtonCoordinator;
    @Mock private HomeButtonCoordinator mHomeButtonCoordinator;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;
    @Mock private NewTabPageDelegate mNewTabPageDelegate;
    @Mock private OptionalButtonCoordinator mOptionalButtonCoordinator;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private ViewTreeObserver mViewTreeObserver;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Captor private ArgumentCaptor<CoordinatorLayout.LayoutParams> mToolbarLayoutParamsCaptor;
    @Captor private ArgumentCaptor<CoordinatorLayout.LayoutParams> mHairlineLayoutParamsCaptor;
    @Captor private ArgumentCaptor<ViewTreeObserver.OnPreDrawListener> mOnPreDrawCaptor;

    private final Supplier<Tab> mTabSupplier = () -> mTab;
    private final SettableNonNullObservableSupplier<Boolean> mCompositorInMotionSupplier =
            ObservableSuppliers.createNonNull(false);
    private final BrowserStateBrowserControlsVisibilityDelegate
            mBrowserStateBrowserControlsVisibilityDelegate =
                    new BrowserStateBrowserControlsVisibilityDelegate(
                            ObservableSuppliers.alwaysFalse());
    private final AtomicInteger mOnResourceRequestedCount = new AtomicInteger();
    private final AtomicInteger mTriggerBitmapCaptureCount = new AtomicInteger();

    private boolean mIsVisible;
    private final BooleanSupplier mIsVisibleSupplier = () -> mIsVisible;

    private boolean mHasTestConstraintsOverride;
    private final SettableNullableObservableSupplier<Integer> mConstraintsSupplier =
            ObservableSuppliers.createNullable();
    private final OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();

    private ToolbarViewResourceAdapter mAdapter;
    private ToolbarControlContainer mControlContainer;
    private TestActivity mActivity;

    private void makeAdapter() {
        mAdapter =
                new ToolbarViewResourceAdapter(mToolbarContainer) {
                    @Override
                    public void onResourceRequested() {
                        // No-op normal functionality and just count calls instead.
                        mOnResourceRequestedCount.getAndIncrement();
                    }

                    @Override
                    public void triggerBitmapCapture() {
                        mTriggerBitmapCaptureCount.getAndIncrement();
                        setDirtyRectEmpty();
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
                mFullscreenManager,
                mToolbarDataProvider,
                mBrowserControlsStateProvider);
        // The adapter may observe some of these already, which will post events.
        RobolectricUtil.runAllBackgroundAndUi();
        // The initial addObserver triggers an event that we don't care about. Reset counts.
        mOnResourceRequestedCount.set(0);
        mTriggerBitmapCaptureCount.set(0);
    }

    private void makeAndInitAdapter() {
        makeAdapter();
        initAdapter();
    }

    private void initControlContainer(@LayoutRes int toolbarLayoutId) {
        mControlContainer =
                (ToolbarControlContainer)
                        mActivity.getLayoutInflater().inflate(R.layout.control_container, null);
        mControlContainer.initWithToolbar(toolbarLayoutId, R.dimen.toolbar_height_no_shadow);
        mControlContainer.setToolbarHairlineForTesting(mToolbarHairline);
        mControlContainer.setPostInitializationDependencies(
                mToolbar,
                mToolbarView,
                false,
                mConstraintsSupplier,
                mTabSupplier,
                mCompositorInMotionSupplier,
                mBrowserStateBrowserControlsVisibilityDelegate,
                mLayoutStateProviderSupplier,
                mFullscreenManager,
                mToolbarDataProvider,
                mBrowserControlsStateProvider,
                mDesktopWindowStateManager);
        ToolbarControlContainer.ToolbarViewResourceCoordinatorLayout toolbarContainer =
                mControlContainer.findViewById(R.id.toolbar_container);
        toolbarContainer.setVisibility(View.GONE);
    }

    private boolean didAdapterLockControls() {
        return mBrowserStateBrowserControlsVisibilityDelegate.get() == BrowserControlsState.SHOWN;
    }

    private void verifyRequestsOnInMotionChange(boolean inMotion, boolean expectResourceRequested) {
        assertNotEquals(inMotion, mCompositorInMotionSupplier.get());
        int requestCount = mOnResourceRequestedCount.get();
        mCompositorInMotionSupplier.set(inMotion);
        RobolectricUtil.runAllBackgroundAndUi();
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
        ResourceFactoryJni.setInstanceForTesting(mResourceFactoryJni);
        when(mToolbarContainer.getWidth()).thenReturn(1);
        when(mToolbarContainer.getHeight()).thenReturn(1);
        when(mToolbarContainer.findViewById(anyInt())).thenReturn(mToolbarHairline);
        when(mToolbarContainer.getViewTreeObserver()).thenReturn(mViewTreeObserver);
        // Run posted Runnables inline so OnPreDrawListener-scheduled captures are
        // observable synchronously in tests.
        when(mToolbarContainer.post(any(Runnable.class)))
                .thenAnswer(
                        inv -> {
                            ((Runnable) inv.getArgument(0)).run();
                            return true;
                        });
        when(mToolbarHairline.getHeight()).thenReturn(1);
        doReturn(mProgressBar).when(mToolbar).getProgressBar();
        doReturn(new CoordinatorLayout.LayoutParams(-1, -1)).when(mToolbarView).getLayoutParams();
        doReturn(new CoordinatorLayout.LayoutParams(-1, -1))
                .when(mToolbarHairline)
                .getLayoutParams();
        mBrowserStateBrowserControlsVisibilityDelegate.set(BrowserControlsState.BOTH);
        mCompositorInMotionSupplier.set(false);
        mBrowserStateBrowserControlsVisibilityDelegate.addSyncObserverAndPostIfNonNull(
                result -> {
                    if (!mHasTestConstraintsOverride) {
                        mConstraintsSupplier.set(result);
                    }
                });
        mActivity = Robolectric.buildActivity(TestActivity.class).get();
    }

    @After
    public void after() {
        mActivity.finish();
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

        UrlBarData urlBarData = UrlBarData.forUrl(JUnitTestGURLs.RED_1);
        when(mToolbarDataProvider.getUrlBarData()).thenReturn(urlBarData);
        mAdapter.triggerBitmapCapture();
        verifyIsDirtyWasBlocked(TopToolbarBlockCaptureReason.VIEW_NOT_DIRTY);
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
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(1, mOnResourceRequestedCount.get());

        // The constraints should no longer block isDirty/captures.
        verifyIsDirtyWasAllowedForSnapshot(difference);

        // Shouldn't be an observer subscribed now, changes shouldn't call onResourceRequested.
        setConstraintsOverride(BrowserControlsState.SHOWN);
        setConstraintsOverride(BrowserControlsState.BOTH);
        assertEquals(1, mOnResourceRequestedCount.get());
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.TOOLBAR_STALE_CAPTURE_BUG_FIX,
        ChromeFeatureList.TOOLBAR_CAPTURE_FIX_FOR_SPAS
    })
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
    @DisableFeatures(ChromeFeatureList.TOOLBAR_STALE_CAPTURE_BUG_FIX)
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
    @DisableFeatures({
        ChromeFeatureList.RECORD_SUPPRESSION_METRICS,
        ChromeFeatureList.TOOLBAR_STALE_CAPTURE_BUG_FIX
    })
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
    @DisableFeatures(ChromeFeatureList.TOOLBAR_STALE_CAPTURE_BUG_FIX)
    public void testIsDirty_InMotion3() {
        makeAndInitAdapter();
        when(mToolbar.isReadyForTextureCapture())
                .thenReturn(
                        CaptureReadinessResult.notReady(
                                TopToolbarBlockCaptureReason
                                        .OPTIONAL_BUTTON_ANIMATION_IN_PROGRESS));
        when(mTab.isNativePage()).thenReturn(false);
        mIsVisible = true;

        verifyRequestsOnInMotionChange(/* inMotion= */ true, /* expectResourceRequested= */ false);
        assertTrue(didAdapterLockControls());

        verifyRequestsOnInMotionChange(/* inMotion= */ false, /* expectResourceRequested= */ true);
        assertFalse(didAdapterLockControls());
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
    @DisableFeatures(ChromeFeatureList.TOOLBAR_STALE_CAPTURE_BUG_FIX)
    public void testInMotion_viewNotVisible() {
        makeAndInitAdapter();
        mockIsReadyDifference(ToolbarSnapshotDifference.URL_TEXT);
        mIsVisible = false;

        verifyRequestsOnInMotionChange(true, false);
    }

    // Drives one frame's pre-draw callback so OnPreDrawListener-scheduled work runs.
    // Tests assert on observable behavior (capture count, no-throw), not on the
    // specific scheduling mechanism.
    private void pumpFrame() {
        verify(mViewTreeObserver, atLeastOnce()).addOnPreDrawListener(mOnPreDrawCaptor.capture());
        mOnPreDrawCaptor.getValue().onPreDraw();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_CAPTURE_FIX_FOR_SPAS)
    public void testInvalidate_whileHidden_producesCapture() {
        makeAndInitAdapter();
        when(mBrowserControlsStateProvider.getTopControlHiddenRatio()).thenReturn(1f);

        mAdapter.invalidate(null);
        pumpFrame();

        assertEquals(1, mTriggerBitmapCaptureCount.get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_CAPTURE_FIX_FOR_SPAS)
    public void testInvalidate_whileHidden_coalescesMultipleInvalidationsPerFrame() {
        makeAndInitAdapter();
        when(mBrowserControlsStateProvider.getTopControlHiddenRatio()).thenReturn(1f);

        mAdapter.invalidate(null);
        mAdapter.invalidate(null);
        mAdapter.invalidate(null);
        pumpFrame();

        assertEquals(1, mTriggerBitmapCaptureCount.get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_CAPTURE_FIX_FOR_SPAS)
    public void testInvalidate_whileVisible_doesNotCapture() {
        makeAndInitAdapter();
        when(mBrowserControlsStateProvider.getTopControlHiddenRatio()).thenReturn(0.5f);

        mAdapter.invalidate(null);

        assertEquals(0, mTriggerBitmapCaptureCount.get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_CAPTURE_FIX_FOR_SPAS)
    public void testInvalidate_stopsCapturingAfterReveal() {
        makeAndInitAdapter();
        when(mBrowserControlsStateProvider.getTopControlHiddenRatio()).thenReturn(1f);
        mAdapter.invalidate(null);
        pumpFrame();
        assertEquals(1, mTriggerBitmapCaptureCount.get());

        // Toolbar starts revealing; subsequent frames must not produce captures.
        when(mBrowserControlsStateProvider.getTopControlHiddenRatio()).thenReturn(0.5f);
        mOnPreDrawCaptor.getValue().onPreDraw();
        mOnPreDrawCaptor.getValue().onPreDraw();

        assertEquals(1, mTriggerBitmapCaptureCount.get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_CAPTURE_FIX_FOR_SPAS)
    public void testInvalidate_afterDestroy_doesNotCapture() {
        makeAndInitAdapter();
        when(mBrowserControlsStateProvider.getTopControlHiddenRatio()).thenReturn(1f);
        mAdapter.destroy();

        mAdapter.invalidate(null);

        assertEquals(0, mTriggerBitmapCaptureCount.get());
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.TOOLBAR_STALE_CAPTURE_BUG_FIX,
        ChromeFeatureList.TOOLBAR_CAPTURE_FIX_FOR_SPAS
    })
    public void testIsDirty_InMotionAndToolbarSwipe() {
        makeAndInitAdapter();
        verifyRequestsOnInMotionChange(true, false);
        mockIsReadyDifference(ToolbarSnapshotDifference.URL_TEXT);
        when(mLayoutStateProvider.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        // The supplier posts the notification so idle to let it through.
        RobolectricUtil.runAllBackgroundAndUi();

        verifyIsDirtyWasBlocked(TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION);

        // TOOLBAR_SWIPE should bypass the in motion check and return dirty.
        when(mLayoutStateProvider.getActiveLayoutType()).thenReturn(LayoutType.TOOLBAR_SWIPE);

        verifyIsDirtyWasAllowedForSnapshot(ToolbarSnapshotDifference.URL_TEXT);
    }

    @Test
    public void testIsDirty_Fullscreen() {
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
        // This is needed for the control container to read the height of the toolbar.
        initControlContainer(R.layout.toolbar_tablet);

        // Set app header with 10px padding on left, 20px on right, and 100px height. Set tab strip
        // height to 80px. Top inset should be 100 - 80 = 20.
        doReturn(80).when(mToolbar).getTabStripHeight();
        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, 0, 80, 100), true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);
        mControlContainer.onHeightChanged(80, 20, false);
        assertNotNull(
                "Control container background is null after app header state change.",
                mControlContainer.getBackground());

        LayerDrawable background = (LayerDrawable) mControlContainer.getBackground();
        final int tabDrawableIndex = 1;
        assertEquals(
                "Left padding for tab drawable is wrong.",
                10,
                background.getLayerInsetLeft(tabDrawableIndex));
        assertEquals(
                "Right padding for tab drawable is wrong.",
                20,
                background.getLayerInsetRight(tabDrawableIndex));
        assertEquals(
                "Top inset for tab drawable is wrong.",
                20,
                background.getLayerInsetTop(tabDrawableIndex));

        // Set app header with 40px height, and tab strip with 50px height.
        // Top inset should be max(0, 40 - 50) = 0.
        appHeaderState = new AppHeaderState(new Rect(0, 0, 100, 40), new Rect(10, 0, 80, 40), true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);
        mControlContainer.onHeightChanged(50, 0, false);
        background = (LayerDrawable) mControlContainer.getBackground();
        assertEquals(
                "Top inset for tab drawable should be 0.",
                0,
                background.getLayerInsetTop(tabDrawableIndex));

        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(new AppHeaderState());
        mControlContainer.onHeightChanged(50, 0, false);
        background = (LayerDrawable) mControlContainer.getBackground();
        assertEquals(
                "Left padding for tab drawable is wrong.",
                0,
                background.getLayerInsetLeft(tabDrawableIndex));
        assertEquals(
                "Right padding for tab drawable is wrong.",
                0,
                background.getLayerInsetRight(tabDrawableIndex));
        assertEquals(
                "Top inset for tab drawable should be 0.",
                0,
                background.getLayerInsetTop(tabDrawableIndex));
    }

    @Test
    public void testTempDrawableAfterCompositorInitialized() {
        initControlContainer(R.layout.toolbar_tablet);
        mControlContainer.setCompositorBackgroundInitialized();
        assertNull(
                "Control container background should be null after app header state change.",
                mControlContainer.getBackground());

        // Set app header with 10px padding on left, 20px on right, and 50px height.
        doReturn(50).when(mToolbar).getTabStripHeight();
        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 50), new Rect(10, 0, 80, 50), true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);
        mControlContainer.onHeightChanged(50, 0, false);
        assertNull(
                "Control container background should not respond to app header state anymore.",
                mControlContainer.getBackground());
    }

    @Test
    public void testTempDrawableInUnfocusedDesktopWindow() {
        initControlContainer(R.layout.toolbar_tablet);

        // Assume that the app started in an unfocused desktop window.
        mControlContainer.setAppInUnfocusedDesktopWindow(true);

        // Simulate invocation of app header state change at startup that sets the temp drawable.
        doReturn(50).when(mToolbar).getTabStripHeight();
        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 50), new Rect(10, 0, 80, 50), true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);
        mControlContainer.onHeightChanged(50, 0, false);

        var backgroundLayerDrawable = (LayerDrawable) mControlContainer.getBackground();
        var stripBackgroundColorDrawable = (ColorDrawable) backgroundLayerDrawable.getDrawable(0);
        assertEquals(
                "Tab strip background color drawable color is incorrect.",
                TabUiThemeUtil.getTabStripBackgroundColor(
                        mActivity,
                        /* isIncognito= */ false,
                        /* isInDesktopWindow= */ true,
                        /* isActivityFocused= */ false),
                stripBackgroundColorDrawable.getColor());
    }

    @Test
    public void testShowLocationBarOnly() {
        doReturn(mLocationBarView).when(mToolbar).removeLocationBarView();
        doReturn(Color.RED).when(mToolbarDataProvider).getPrimaryColor();
        ToolbarControlContainer controlContainer =
                (ToolbarControlContainer)
                        mActivity.getLayoutInflater().inflate(R.layout.control_container, null);
        controlContainer.initWithToolbar(R.layout.toolbar_phone, R.dimen.toolbar_height_no_shadow);
        controlContainer.setPostInitializationDependencies(
                mToolbar,
                mToolbarView,
                false,
                mConstraintsSupplier,
                mTabSupplier,
                mCompositorInMotionSupplier,
                mBrowserStateBrowserControlsVisibilityDelegate,
                mLayoutStateProviderSupplier,
                mFullscreenManager,
                mToolbarDataProvider,
                mBrowserControlsStateProvider,
                null);

        ToolbarPhone toolbarPhone = controlContainer.findViewById(R.id.toolbar);
        doReturn(mLocationBarCoordinatorPhone).when(mLocationBarCoordinator).getPhoneCoordinator();
        doReturn(mNewTabPageDelegate).when(mToolbarDataProvider).getNewTabPageDelegate();
        doReturn(new GURL(UrlConstants.ABOUT_URL)).when(mToolbarDataProvider).getCurrentGurl();
        toolbarPhone.setLocationBarCoordinator(mLocationBarCoordinator);
        toolbarPhone.initialize(
                mToolbarDataProvider,
                null,
                mMenuButtonCoordinator,
                mTabSwitcherButtonCoordinator,
                null,
                null,
                null,
                mProgressBar,
                mReloadButtonCoordinator,
                mBackButtonCoordinator,
                mForwardButtonCoordinator,
                mHomeButtonCoordinator,
                /* signinButtonCoordinator= */ null,
                mThemeColorProvider,
                mIncognitoStateProvider,
                /* incognitoWindowCountSupplier= */ null,
                mWindowAndroid);

        controlContainer.toggleLocationBarOnlyMode(true);
        verify(mProgressBar).setVisibility(View.GONE);
        verify(mToolbarView).setVisibility(View.GONE);
        verify(mToolbarView).removeView(mLocationBarView);

        assertEquals(Color.RED, ((ColorDrawable) controlContainer.getBackground()).getColor());
        doReturn(Color.GREEN).when(mToolbarDataProvider).getPrimaryColor();
        controlContainer.onPrimaryColorChanged();
        assertEquals(Color.GREEN, ((ColorDrawable) controlContainer.getBackground()).getColor());

        ToolbarViewResourceCoordinatorLayout toolbarViewResourceFrameLayout =
                controlContainer.getToolbarContainerForTesting();
        assertEquals(
                mLocationBarView,
                toolbarViewResourceFrameLayout.getChildAt(
                        toolbarViewResourceFrameLayout.getChildCount() - 1));

        MarginLayoutParams layoutParams = new MarginLayoutParams(500, 100);
        doReturn(layoutParams).when(mLocationBarCoordinatorPhone).getMarginLayoutParams();
        controlContainer.toggleLocationBarOnlyMode(false);
        verify(mProgressBar).setVisibility(View.VISIBLE);
        verify(mToolbarView).setVisibility(View.VISIBLE);
        verify(mToolbar).restoreLocationBarView();
        assertEquals(
                Color.TRANSPARENT, ((ColorDrawable) controlContainer.getBackground()).getColor());
    }

    @Test
    public void testInterceptTouchEvent() {
        ToolbarControlContainer controlContainer =
                (ToolbarControlContainer)
                        mActivity.getLayoutInflater().inflate(R.layout.control_container, null);
        controlContainer.initWithToolbar(R.layout.toolbar_phone, R.dimen.toolbar_height_no_shadow);
        controlContainer.setPostInitializationDependencies(
                mToolbar,
                mToolbarView,
                false,
                mConstraintsSupplier,
                mTabSupplier,
                mCompositorInMotionSupplier,
                mBrowserStateBrowserControlsVisibilityDelegate,
                mLayoutStateProviderSupplier,
                mFullscreenManager,
                mToolbarDataProvider,
                mBrowserControlsStateProvider,
                null);
        ToolbarControlContainer.ToolbarViewResourceCoordinatorLayout toolbarContainer =
                controlContainer.findViewById(R.id.toolbar_container);
        toolbarContainer.setVisibility(View.GONE);

        MotionEvent clickEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);

        assertTrue(controlContainer.onInterceptTouchEvent(clickEvent));

        toolbarContainer.setVisibility(View.VISIBLE);
        doReturn(100).when(mToolbar).getTabStripHeight();
        assertTrue(controlContainer.onInterceptTouchEvent(clickEvent));

        doReturn(0).when(mToolbar).getTabStripHeight();
        controlContainer.addTouchEventObserver(mTouchEventObserver);
        assertFalse(controlContainer.onInterceptTouchEvent(clickEvent));

        doReturn(true).when(mTouchEventObserver).onInterceptTouchEvent(clickEvent);
        assertTrue(controlContainer.onInterceptTouchEvent(clickEvent));
    }

    @Test
    public void testHeightSupplier() {
        var controlContainer = new ToolbarControlContainer(mActivity, null);
        var heightSupplier = ObservableSuppliers.createNonNull(-1);
        controlContainer.setOnHeightChangedListener(heightSupplier);
        controlContainer.onSizeChanged(100, 200, 100, 100);
        assertEquals(200, (int) heightSupplier.get());
    }

    @Test
    public void testHeightSupplier_noHeightChange() {
        var controlContainer = new ToolbarControlContainer(mActivity, null);
        var heightSupplier = ObservableSuppliers.createNonNull(-1);
        controlContainer.setOnHeightChangedListener(heightSupplier);
        controlContainer.onSizeChanged(100, 100, 100, 100);
        assertEquals(-1, (int) heightSupplier.get());
    }

    @Test
    public void testStaleCapturedUrlOnScroll_Stale() {
        ResettersForTesting.register(
                ToolbarControlContainer.forceStaleCaptureHistogramForTesting());
        makeAndInitAdapter();
        mConstraintsSupplier.set(BrowserControlsState.BOTH);

        UrlBarData urlBarData1 = UrlBarData.forUrl(JUnitTestGURLs.RED_1);
        when(mToolbarDataProvider.getUrlBarData()).thenReturn(urlBarData1);
        mAdapter.onCaptureEnd();

        UrlBarData urlBarData2 = UrlBarData.forUrl(JUnitTestGURLs.RED_2);
        when(mToolbarDataProvider.getUrlBarData()).thenReturn(urlBarData2);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.Toolbar.StaleCapturedUrlOnScroll.Subsampled", 1)
                        .build();
        mAdapter.onContentViewScrollingStateChanged(true);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testStaleCapturedUrlOnScroll_NotStale() {
        ResettersForTesting.register(
                ToolbarControlContainer.forceStaleCaptureHistogramForTesting());
        makeAndInitAdapter();
        mConstraintsSupplier.set(BrowserControlsState.BOTH);

        UrlBarData urlBarData1 = UrlBarData.forUrl(JUnitTestGURLs.RED_1);
        when(mToolbarDataProvider.getUrlBarData()).thenReturn(urlBarData1);
        mAdapter.onCaptureEnd();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.Toolbar.StaleCapturedUrlOnScroll.Subsampled", 0)
                        .build();
        mAdapter.onContentViewScrollingStateChanged(true);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testStaleCapturedUrlOnScroll_ControlsLocked() {
        ResettersForTesting.register(
                ToolbarControlContainer.forceStaleCaptureHistogramForTesting());
        makeAndInitAdapter();
        setConstraintsOverride(BrowserControlsState.SHOWN);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.Toolbar.StaleCapturedUrlOnScroll.Subsampled")
                        .build();
        mAdapter.onContentViewScrollingStateChanged(true);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testStaleCapturedUrlOnScroll_NotScrolling() {
        ResettersForTesting.register(
                ToolbarControlContainer.forceStaleCaptureHistogramForTesting());
        makeAndInitAdapter();
        mConstraintsSupplier.set(BrowserControlsState.BOTH);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.Toolbar.StaleCapturedUrlOnScroll.Subsampled")
                        .build();
        mAdapter.onContentViewScrollingStateChanged(false);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnHeightTransition_ShowTabStrip() {
        initControlContainer(R.layout.toolbar_tablet);
        mControlContainer.setMinimumHeight(0);

        Resources res = mActivity.getResources();
        int tabStripHeight = res.getDimensionPixelSize(R.dimen.tab_strip_height);
        int toolbarHeight = res.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        int hairlineHeight = res.getDimensionPixelSize(R.dimen.toolbar_hairline_height);
        doReturn(0).when(mToolbar).getTabStripHeight();
        doReturn(toolbarHeight).when(mToolbar).getHeight();
        doReturn(hairlineHeight).when(mToolbarHairline).getHeight();

        // Start transition
        mControlContainer.onHeightChanged(tabStripHeight, 0, true);

        verify(mToolbarView).setLayoutParams(mToolbarLayoutParamsCaptor.capture());
        verify(mToolbarHairline).setLayoutParams(mHairlineLayoutParamsCaptor.capture());
        assertEquals(
                "Toolbar top margin is wrong.",
                tabStripHeight,
                mToolbarLayoutParamsCaptor.getValue().topMargin);
        assertEquals(
                "Hairline top margin is wrong.",
                tabStripHeight + toolbarHeight,
                mHairlineLayoutParamsCaptor.getValue().topMargin);

        // Finish transition
        mControlContainer.onHeightTransitionFinished(true);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(
                "MinHeight is not set correctly.",
                toolbarHeight + tabStripHeight + hairlineHeight,
                mControlContainer.getMinimumHeight());
    }

    @Test
    public void testOnHeightTransition_HideTabStrip() {
        initControlContainer(R.layout.toolbar_tablet);
        mControlContainer.setMinimumHeight(0);

        Resources res = mActivity.getResources();
        int tabStripHeight = res.getDimensionPixelSize(R.dimen.tab_strip_height);
        int toolbarHeight = res.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        int hairlineHeight = res.getDimensionPixelSize(R.dimen.toolbar_hairline_height);
        doReturn(tabStripHeight).when(mToolbar).getTabStripHeight();
        doReturn(toolbarHeight).when(mToolbar).getHeight();
        doReturn(hairlineHeight).when(mToolbarHairline).getHeight();

        mControlContainer.onHeightChanged(0, 0, true);

        verify(mToolbarView).setLayoutParams(mToolbarLayoutParamsCaptor.capture());
        verify(mToolbarHairline).setLayoutParams(mHairlineLayoutParamsCaptor.capture());

        assertEquals(
                "Toolbar top margin is wrong.", 0, mToolbarLayoutParamsCaptor.getValue().topMargin);
        assertEquals(
                "Hairline top margin is wrong.",
                toolbarHeight,
                mHairlineLayoutParamsCaptor.getValue().topMargin);

        // Finish transition
        mControlContainer.onHeightTransitionFinished(true);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(
                "MinHeight is not set correctly.",
                toolbarHeight + hairlineHeight,
                mControlContainer.getMinimumHeight());
    }

    @Test
    public void testOnHeightTransition_TransitionCanceled() {
        initControlContainer(R.layout.toolbar_tablet);
        mControlContainer.setMinimumHeight(0);

        Resources res = mActivity.getResources();
        int tabStripHeight = res.getDimensionPixelSize(R.dimen.tab_strip_height);
        int toolbarHeight = res.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        int hairlineHeight = res.getDimensionPixelSize(R.dimen.toolbar_hairline_height);
        doReturn(tabStripHeight).when(mToolbar).getTabStripHeight();
        doReturn(toolbarHeight).when(mToolbar).getHeight();
        doReturn(hairlineHeight).when(mToolbarHairline).getHeight();

        mControlContainer.onHeightChanged(0, 0, true);

        verify(mToolbarView).setLayoutParams(mToolbarLayoutParamsCaptor.capture());
        verify(mToolbarHairline).setLayoutParams(mHairlineLayoutParamsCaptor.capture());

        assertEquals(
                "Toolbar top margin is wrong.", 0, mToolbarLayoutParamsCaptor.getValue().topMargin);
        assertEquals(
                "Hairline top margin is wrong.",
                toolbarHeight,
                mHairlineLayoutParamsCaptor.getValue().topMargin);

        // Finish transition
        mControlContainer.onHeightTransitionFinished(false);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(
                "Transition not finished, so minHeight stays the same.",
                0,
                mControlContainer.getMinimumHeight());
    }

    @Test
    public void testDoSynchronousLayout() {
        initControlContainer(R.layout.toolbar_phone);
        ViewResourceAdapter mockAdapter = mock(ViewResourceAdapter.class);

        ToolbarControlContainer spyContainer = spy(mControlContainer);
        doReturn(mockAdapter).when(spyContainer).getToolbarResourceAdapter();

        // Test with forceCaptureAfterLayout = false
        spyContainer.doSynchronousLayout(false);
        verify(spyContainer).measure(anyInt(), anyInt());
        verify(spyContainer).layout(anyInt(), anyInt(), anyInt(), anyInt());
        verify(mockAdapter, never()).invalidate(null);
        verify(mockAdapter, never()).triggerBitmapCapture();

        // Test with forceCaptureAfterLayout = true
        spyContainer.doSynchronousLayout(true);
        verify(mockAdapter).invalidate(null);
        verify(mockAdapter).triggerBitmapCapture();
    }

    @Test
    public void testUpdateButtonVisibility_TransitionsNtp() {
        initControlContainer(R.layout.toolbar_phone);
        ToolbarPhone toolbarPhone = mControlContainer.findViewById(R.id.toolbar);
        toolbarPhone.setMenuButtonCoordinatorForTesting(mMenuButtonCoordinator);

        // Transition away from NTP.
        doReturn(JUnitTestGURLs.RED_1).when(mToolbarDataProvider).getCurrentGurl();
        toolbarPhone.updateButtonVisibility();
        verify(mMenuButtonCoordinator).setVisibility(true);

        // Transition to regular NTP.
        doReturn(JUnitTestGURLs.NTP_URL).when(mToolbarDataProvider).getCurrentGurl();
        toolbarPhone.updateButtonVisibility();
        // Since isNtp becomes true, typical outcomes apply.
    }

    @Test
    public void testUpdateOptionalButton_TransitionsNtp() {
        initControlContainer(R.layout.toolbar_phone);
        ToolbarPhone toolbarPhone = mControlContainer.findViewById(R.id.toolbar);
        toolbarPhone.setThemeColorProvider(mThemeColorProvider);
        toolbarPhone.setOptionalButtonCoordinatorForTesting(mOptionalButtonCoordinator);

        ButtonData buttonData = mock(ButtonData.class);
        toolbarPhone.updateOptionalButton(buttonData);

        verify(mOptionalButtonCoordinator).updateButton(eq(buttonData), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testUpdateOptionalButton_DelegatesToLocationBar() {
        initControlContainer(R.layout.toolbar_phone);
        ToolbarPhone toolbarPhone = mControlContainer.findViewById(R.id.toolbar);
        toolbarPhone.setThemeColorProvider(mThemeColorProvider);
        toolbarPhone.setLocationBarCoordinator(mLocationBarCoordinator);

        // NOTE: In this test mOptionalButtonCoordinator is never created.

        ButtonData buttonData = mock(ButtonData.class);
        toolbarPhone.updateOptionalButton(buttonData);

        verify(mLocationBarCoordinator).updateOptionalButton(eq(buttonData));
        verify(mOptionalButtonCoordinator, never()).updateButton(any(), anyBoolean());

        toolbarPhone.hideOptionalButton();

        verify(mLocationBarCoordinator).hideOptionalButton();
        verify(mOptionalButtonCoordinator, never()).hideButton();
    }

    @Test
    @DisableFeatures(SigninFeatures.SIGNIN_LEVEL_UP_BUTTON)
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testUpdateOptionalButton_OnNtp_UpdatesToolbarButton() {
        initControlContainer(R.layout.toolbar_phone);
        ToolbarPhone toolbarPhone = mControlContainer.findViewById(R.id.toolbar);
        toolbarPhone.setThemeColorProvider(mThemeColorProvider);
        toolbarPhone.setOptionalButtonCoordinatorForTesting(mOptionalButtonCoordinator);
        toolbarPhone.setLocationBarCoordinator(mLocationBarCoordinator);

        doReturn(true).when(mNewTabPageDelegate).isCurrentlyVisible();
        toolbarPhone.mVisualState = ToolbarPhone.VisualState.NEW_TAB_NORMAL;

        ButtonData buttonData = mock(ButtonData.class);
        toolbarPhone.updateOptionalButton(buttonData);

        verify(mLocationBarCoordinator, never()).updateOptionalButton(any());
        verify(mLocationBarCoordinator).hideOptionalButton();
        verify(mOptionalButtonCoordinator).updateButton(eq(buttonData), anyBoolean());

        toolbarPhone.hideOptionalButton();

        verify(mLocationBarCoordinator, times(2)).hideOptionalButton();
        verify(mOptionalButtonCoordinator).hideButton();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_SNAPSHOT_REFACTOR)
    public void testOnMeasure_WithToolBarSnapShotRefactorEnabled_SetsCorrectMargins() {
        // Top margins (toolbar_container, toolbar, hairline) when the flag is enabled:
        // tab strip height, 0, toolbar height

        Resources res = mActivity.getResources();
        int toolbarLayoutHeight = res.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        int simulatedTabStripHeight = 105;

        checkOnMeasureMargins(
                simulatedTabStripHeight,
                toolbarLayoutHeight,
                /* expectedContainerTopMargin= */ simulatedTabStripHeight,
                /* expectedHairlineTopMargin= */ toolbarLayoutHeight);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TOOLBAR_SNAPSHOT_REFACTOR)
    public void testOnMeasure_WithToolBarSnapShotRefactorDisabled_LeavesMarginsUntouched() {
        // Top margins (toolbar_container, toolbar, hairline) when the flag is disabled:
        // 0, tab strip height, tab strip height + toolbar height

        Resources res = mActivity.getResources();
        int toolbarLayoutHeight = res.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        int simulatedTabStripHeight = 105;

        checkOnMeasureMargins(
                simulatedTabStripHeight,
                toolbarLayoutHeight,
                /* expectedContainerTopMargin= */ 0,
                /* expectedHairlineTopMargin= */ simulatedTabStripHeight + toolbarLayoutHeight);
    }

    private void checkOnMeasureMargins(
            int simulatedTabStripHeight,
            int toolbarLayoutHeight,
            int expectedContainerTopMargin,
            int expectedHairlineTopMargin) {
        initControlContainer(R.layout.toolbar_tablet);

        doReturn(simulatedTabStripHeight).when(mToolbar).getTabStripHeight();

        View toolbarContainerView = mControlContainer.findViewById(R.id.toolbar_container);
        View hairlineView = mControlContainer.findViewById(R.id.toolbar_hairline);

        // Get the existing layout params.
        MarginLayoutParams oldToolbarContainerParams =
                (MarginLayoutParams) toolbarContainerView.getLayoutParams();
        oldToolbarContainerParams.topMargin = 0;
        toolbarContainerView.setLayoutParams(oldToolbarContainerParams);

        MarginLayoutParams oldHairlineParams = (MarginLayoutParams) hairlineView.getLayoutParams();
        oldHairlineParams.topMargin = simulatedTabStripHeight + toolbarLayoutHeight;
        hairlineView.setLayoutParams(oldHairlineParams);

        // Execute the onMeasure pass that we overrode.
        int widthSpec = View.MeasureSpec.makeMeasureSpec(1024, View.MeasureSpec.EXACTLY);
        int heightSpec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
        mControlContainer.measure(widthSpec, heightSpec);

        // Read back the final layout parameters.
        MarginLayoutParams newToolbarContainerParams =
                (MarginLayoutParams) toolbarContainerView.getLayoutParams();
        MarginLayoutParams newHairlineParams = (MarginLayoutParams) hairlineView.getLayoutParams();

        assertEquals(
                "Toolbar container top margin is incorrect.",
                expectedContainerTopMargin,
                newToolbarContainerParams.topMargin);

        assertEquals(
                "Hairline top margin is incorrect.",
                expectedHairlineTopMargin,
                newHairlineParams.topMargin);
    }
}
