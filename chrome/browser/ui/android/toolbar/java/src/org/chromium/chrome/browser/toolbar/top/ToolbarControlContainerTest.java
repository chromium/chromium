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
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.annotation.LayoutRes;
import androidx.coordinatorlayout.widget.CoordinatorLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

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
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
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
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.BiConsumer;
import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/** Unit tests for {@link ToolbarControlContainer}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({SigninFeatures.SIGNIN_LEVEL_UP_BUTTON})
public class ToolbarControlContainerTest {
    private static final String BLOCK_NAME = "Android.TopToolbar.BlockCaptureReason";
    private static final String ALLOW_NAME = "Android.TopToolbar.AllowCaptureReason";
    private static final String DIFFERENCE_NAME = "Android.TopToolbar.SnapshotDifference";
    private static final String MOTION_STAGE_NAME = "Android.TopToolbar.InMotionStage";

    @Rule public MockitoRule rule = MockitoJUnit.rule();

    @Mock private ResourceFactory.Natives mResourceFactoryJni;
    @Mock private Toolbar mToolbar;
    @Mock private ToggleTabStackButtonCoordinator mTabSwitcherButtonCoordinator;
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
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private BiConsumer<Integer, Integer> mHorizontalMarginCallback;

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

    private FrameLayout mToolbarContainer;
    private ViewGroup mToolbarView;
    private View mLocationBarView;
    private ToolbarHairlineView mToolbarHairline;
    private ToolbarProgressBar mProgressBar;
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
                mTopControlsStacker);
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
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.control_container, /* root= */ null);
        mControlContainer.initWithToolbar(toolbarLayoutId, R.dimen.toolbar_height_no_shadow);
        mToolbarHairline = mControlContainer.findViewById(R.id.toolbar_hairline);
        mToolbarHairline.layout(0, 0, 1, 1);
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
                mDesktopWindowStateManager,
                mTopControlsStacker);
        ToolbarControlContainer.ToolbarViewResourceCoordinatorLayout toolbarContainer =
                mControlContainer.findViewById(R.id.toolbar_container);
        toolbarContainer.setVisibility(View.GONE);
    }

    /**
     * Inits the control container with a visible, laid out toolbar container and swipe handler.
     *
     * @return The mocked swipe handler set on the control container.
     */
    private SwipeHandler initControlContainerForBelowToolbarTouch() {
        initControlContainer(R.layout.toolbar_phone);
        ToolbarViewResourceCoordinatorLayout toolbarContainer =
                mControlContainer.findViewById(R.id.toolbar_container);
        toolbarContainer.setVisibility(View.VISIBLE);
        mControlContainer.layout(0, 0, 1000, 180);
        toolbarContainer.layout(0, 0, 1000, 100);
        SwipeHandler swipeHandler = mock(SwipeHandler.class);
        mControlContainer.setSwipeHandler(swipeHandler);
        return swipeHandler;
    }

    /**
     * Sends a horizontal drag at the given height through onInterceptTouchEvent() and asserts that
     * none of its events are intercepted.
     */
    private void assertHorizontalDragNotIntercepted(int y) {
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 650, y, 0);
        assertFalse(mControlContainer.onInterceptTouchEvent(downEvent));
        for (int i = 1; i <= 5; i++) {
            MotionEvent moveEvent =
                    MotionEvent.obtain(0, i * 10, MotionEvent.ACTION_MOVE, 650 + i * 60, y, 0);
            assertFalse(mControlContainer.onInterceptTouchEvent(moveEvent));
        }
    }

    /** Adds a generic visible child, such as the tab sharing toolbar, to the control container. */
    private FrameLayout addStackedChild() {
        FrameLayout stackedChild = new FrameLayout(mActivity);
        stackedChild.setVisibility(View.VISIBLE);
        mControlContainer.addView(stackedChild);
        return stackedChild;
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
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();

        mToolbarContainer = new FrameLayout(mActivity);
        mToolbarHairline = new ToolbarHairlineView(mActivity, /* attrs= */ null);
        mToolbarHairline.setId(R.id.toolbar_hairline);
        mToolbarHairline.setLayoutParams(
                new CoordinatorLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        mToolbarHairline.layout(0, 0, 1, 1);
        mToolbarContainer.addView(mToolbarHairline);
        mActivity.setContentView(mToolbarContainer);
        mToolbarContainer.layout(0, 0, 1, 1);

        mToolbarView = new FrameLayout(mActivity);
        mToolbarView.setLayoutParams(
                new CoordinatorLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        mLocationBarView = new View(mActivity);
        mProgressBar = new ToolbarProgressBar(mActivity, /* attrs= */ null);
        doReturn(mProgressBar).when(mToolbar).getProgressBar();

        mBrowserStateBrowserControlsVisibilityDelegate.set(BrowserControlsState.BOTH);
        mCompositorInMotionSupplier.set(false);
        mBrowserStateBrowserControlsVisibilityDelegate.addSyncObserverAndPostIfNonNull(
                result -> {
                    if (!mHasTestConstraintsOverride) {
                        mConstraintsSupplier.set(result);
                    }
                });
    }

    @After
    public void after() {
        VerticalTabUtils.resetSharedPrefsForTesting();
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
    @DisableFeatures(ChromeFeatureList.TOOLBAR_CAPTURE_FIX_FOR_SPAS)
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
    @DisableFeatures(ChromeFeatureList.TOOLBAR_CAPTURE_FIX_FOR_SPAS)
    public void testInMotion_viewNotVisible() {
        makeAndInitAdapter();
        mockIsReadyDifference(ToolbarSnapshotDifference.URL_TEXT);
        mIsVisible = false;

        verifyRequestsOnInMotionChange(true, false);
        assertEquals(0, mTriggerBitmapCaptureCount.get());
        assertFalse(didAdapterLockControls());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_CAPTURE_FIX_FOR_SPAS)
    public void testInMotion_viewNotVisible_capturesOnMotionStart() {
        makeAndInitAdapter();
        mockIsReadyDifference(ToolbarSnapshotDifference.URL_TEXT);
        when(mTab.isNativePage()).thenReturn(false);
        mIsVisible = false;

        verifyRequestsOnInMotionChange(/* inMotion= */ true, /* expectResourceRequested= */ false);
        assertEquals(1, mTriggerBitmapCaptureCount.get());
        assertFalse(didAdapterLockControls());

        // The capture cleaned the view, so later motion doesn't capture again until the next
        // invalidation.
        verifyRequestsOnInMotionChange(/* inMotion= */ false, /* expectResourceRequested= */ true);
        verifyRequestsOnInMotionChange(/* inMotion= */ true, /* expectResourceRequested= */ false);
        assertEquals(1, mTriggerBitmapCaptureCount.get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_CAPTURE_FIX_FOR_SPAS)
    public void testInMotion_viewNotVisible_snapshotSame() {
        makeAndInitAdapter();
        when(mToolbar.isReadyForTextureCapture())
                .thenReturn(
                        CaptureReadinessResult.notReady(
                                TopToolbarBlockCaptureReason.SNAPSHOT_SAME));
        when(mTab.isNativePage()).thenReturn(false);
        mIsVisible = false;

        verifyRequestsOnInMotionChange(/* inMotion= */ true, /* expectResourceRequested= */ false);
        assertEquals(0, mTriggerBitmapCaptureCount.get());
        assertTrue(mAdapter.getDirtyRect().isEmpty());
        assertFalse(didAdapterLockControls());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_CAPTURE_FIX_FOR_SPAS)
    public void testInMotion_viewNotVisible_notReady() {
        makeAndInitAdapter();
        when(mToolbar.isReadyForTextureCapture())
                .thenReturn(
                        CaptureReadinessResult.notReady(
                                TopToolbarBlockCaptureReason
                                        .OPTIONAL_BUTTON_ANIMATION_IN_PROGRESS));
        when(mTab.isNativePage()).thenReturn(false);
        mIsVisible = false;

        verifyRequestsOnInMotionChange(/* inMotion= */ true, /* expectResourceRequested= */ false);
        assertEquals(0, mTriggerBitmapCaptureCount.get());
        assertFalse(didAdapterLockControls());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_CAPTURE_FIX_FOR_SPAS)
    public void testInMotion_viewNotVisible_controlsLocked() {
        makeAndInitAdapter();
        mockIsReadyDifference(ToolbarSnapshotDifference.URL_TEXT);
        when(mTab.isNativePage()).thenReturn(false);
        setConstraintsOverride(BrowserControlsState.SHOWN);
        mIsVisible = false;

        verifyRequestsOnInMotionChange(/* inMotion= */ true, /* expectResourceRequested= */ false);
        assertEquals(0, mTriggerBitmapCaptureCount.get());
        assertFalse(didAdapterLockControls());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TOOLBAR_CAPTURE_FIX_FOR_SPAS)
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
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    public void testToolbarRightOffsetInDesktopWindow() {
        initControlContainer(R.layout.toolbar_tablet);
        mControlContainer.setToolbarHorizontalMarginCallback(mHorizontalMarginCallback);

        VerticalTabUtils.setVerticalTabsEnabled(true);
        SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier =
                ObservableSuppliers.createNonNull(true);
        mControlContainer.setIsVerticalTabsActiveSupplier(isVerticalTabsActiveSupplier);

        // Initially, tab strip is visible (height 80). Callback should be called with (0, 0)
        // but is skipped since the initial margins are already 0.
        mControlContainer.onHeightChanged(80, 20, false);
        verify(mHorizontalMarginCallback, never()).accept(0, 0);
        assertFalse(mControlContainer.isToolbarInAppHeader());

        // Set app header with 10px padding on left, 20px on right, and 100px height.
        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, 0, 80, 100), true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);

        // Vertical tabs active and tab strip hidden (height 0). Callback should be called with
        // (0, 20).
        mControlContainer.onHeightChanged(0, 20, false);
        verify(mHorizontalMarginCallback).accept(0, 20);
        assertTrue(mControlContainer.isToolbarInAppHeader());

        // Disable vertical tabs while tab strip height is 0. Shifting is retained.
        VerticalTabUtils.setVerticalTabsEnabled(false);
        isVerticalTabsActiveSupplier.set(false);
        verify(mHorizontalMarginCallback, never()).accept(0, 0);

        // Tab strip expands. Shifting is undone.
        mControlContainer.onHeightChanged(80, 20, false);
        verify(mHorizontalMarginCallback).accept(0, 0);
        assertFalse(mControlContainer.isToolbarInAppHeader());

        // Exit desktop window. Margin is still 0, so callback should not be called again.
        var appHeaderState2 =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, 0, 80, 100), false);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState2);
        mControlContainer.onAppHeaderStateChanged(appHeaderState2);
        verify(mHorizontalMarginCallback).accept(0, 0);
        assertFalse(mControlContainer.isToolbarInAppHeader());
    }

    @Test
    public void testToolbarRightOffset_StartupWithVerticalTabsOff() {
        initControlContainer(R.layout.toolbar_tablet);
        mControlContainer.setToolbarHorizontalMarginCallback(mHorizontalMarginCallback);

        SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier =
                ObservableSuppliers.createNonNull(false);
        mControlContainer.setIsVerticalTabsActiveSupplier(isVerticalTabsActiveSupplier);

        // On startup in desktop window, mTabStripHeight is still 0.
        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, 0, 80, 100), true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);
        mControlContainer.onAppHeaderStateChanged(appHeaderState);

        assertEquals(0, mControlContainer.getRightMarginForTesting());
        verify(mHorizontalMarginCallback, never()).accept(anyInt(), anyInt());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    public void testToolbarRightOffset_ActiveWhileTabStripVisible() {
        initControlContainer(R.layout.toolbar_tablet);
        mControlContainer.setToolbarHorizontalMarginCallback(mHorizontalMarginCallback);

        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, 0, 80, 100), true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);

        SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier =
                ObservableSuppliers.createNonNull(false);
        mControlContainer.setIsVerticalTabsActiveSupplier(isVerticalTabsActiveSupplier);

        // Tab strip is visible (height 80). Margin is 0.
        mControlContainer.onHeightChanged(80, 20, false);
        assertEquals(0, mControlContainer.getRightMarginForTesting());
        verify(mHorizontalMarginCallback, never()).accept(anyInt(), anyInt());

        // Vertical tabs becomes active. Right margin is updated immediately even while
        // tab strip is still visible.
        VerticalTabUtils.setVerticalTabsEnabled(true);
        mControlContainer.updateToolbarHorizontalMargins();
        assertEquals(20, mControlContainer.getRightMarginForTesting());
        verify(mHorizontalMarginCallback).accept(0, 20);

        // Intermediate animation height update preserves the right margin.
        mControlContainer.onHeightChanged(40, 20, false);
        assertEquals(20, mControlContainer.getRightMarginForTesting());

        // Tab strip finishes collapse to height 0.
        mControlContainer.onHeightChanged(0, 20, false);
        assertEquals(20, mControlContainer.getRightMarginForTesting());
        verify(mHorizontalMarginCallback, times(1)).accept(0, 20);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    public void testToolbarRightOffset_StartupWithVerticalTabsOn() {
        VerticalTabUtils.setVerticalTabsEnabled(true);
        initControlContainer(R.layout.toolbar_tablet);

        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, 0, 80, 100), true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);

        SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier =
                ObservableSuppliers.createNonNull(true);
        mControlContainer.setIsVerticalTabsActiveSupplier(isVerticalTabsActiveSupplier);
        mControlContainer.setToolbarHorizontalMarginCallback(mHorizontalMarginCallback);

        assertEquals(20, mControlContainer.getRightMarginForTesting());
        verify(mHorizontalMarginCallback).accept(0, 20);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    public void testToolbarRightOffset_VerticalTabsEnabledViaPref() {
        initControlContainer(R.layout.toolbar_tablet);
        mControlContainer.setToolbarHorizontalMarginCallback(mHorizontalMarginCallback);

        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, 0, 80, 100), true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);

        // Supplier is false (e.g. animation hasn't completed yet).
        SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier =
                ObservableSuppliers.createNonNull(false);
        mControlContainer.setIsVerticalTabsActiveSupplier(isVerticalTabsActiveSupplier);
        assertEquals(0, mControlContainer.getRightMarginForTesting());

        // Enable vertical tabs via preference.
        VerticalTabUtils.setVerticalTabsEnabled(true);
        mControlContainer.updateToolbarHorizontalMargins();

        // Right offset is applied immediately because VerticalTabUtils.isVerticalTabsEnabled is
        // true.
        assertEquals(20, mControlContainer.getRightMarginForTesting());
        verify(mHorizontalMarginCallback).accept(0, 20);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    public void testToolbarLeftOffsetInDesktopWindow() {
        initControlContainer(R.layout.toolbar_tablet);
        mControlContainer.setToolbarHorizontalMarginCallback(mHorizontalMarginCallback);

        VerticalTabUtils.setVerticalTabsEnabled(true);
        SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier =
                ObservableSuppliers.createNonNull(true);
        mControlContainer.setIsVerticalTabsActiveSupplier(isVerticalTabsActiveSupplier);

        // Set app header with 180px padding on left (e.g. app icon + app title + dropdown in
        // fullscreen/split-screen), 20px on right, and 100px height.
        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 500, 100), new Rect(180, 0, 480, 100), true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);

        // When Vertical Tabs is expanded (240px >= 180px leftPadding), leftMargin is 0 and
        // rightMargin is 20.
        SettableNonNullObservableSupplier<Integer> vtWidthSupplier =
                ObservableSuppliers.createNonNull(240);
        mControlContainer.setVerticalTabsContainerWidthSupplier(vtWidthSupplier);
        assertEquals(0, mControlContainer.getLeftMarginForTesting());
        assertEquals(20, mControlContainer.getRightMarginForTesting());
        verify(mHorizontalMarginCallback).accept(0, 20);

        // When Vertical Tabs collapses (52px < 180px leftPadding), leftMargin is 180 - 52 = 128px.
        vtWidthSupplier.set(52);
        assertEquals(128, mControlContainer.getLeftMarginForTesting());
        verify(mHorizontalMarginCallback).accept(128, 20);

        // When Vertical Tabs expands again (240px >= 180px), leftMargin returns to 0.
        vtWidthSupplier.set(240);
        assertEquals(0, mControlContainer.getLeftMarginForTesting());
        verify(mHorizontalMarginCallback, times(2)).accept(0, 20);

        // Collapse again (leftMargin = 128), then disable Vertical Tabs while tabStripHeight is 0.
        // Shifting is retained until tab strip expands.
        vtWidthSupplier.set(52);
        VerticalTabUtils.setVerticalTabsEnabled(false);
        isVerticalTabsActiveSupplier.set(false);
        assertEquals(128, mControlContainer.getLeftMarginForTesting());
        assertEquals(20, mControlContainer.getRightMarginForTesting());
        verify(mHorizontalMarginCallback, never()).accept(0, 0);

        // Tab strip expands (height 80). Shifting is undone.
        mControlContainer.onHeightChanged(80, 20, false);
        assertEquals(0, mControlContainer.getLeftMarginForTesting());
        assertEquals(0, mControlContainer.getRightMarginForTesting());
        verify(mHorizontalMarginCallback).accept(0, 0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_SNAPSHOT_REFACTOR)
    public void testSetToolbarContainerTopMarginForAutoHiddenVerticalTab_RefactorEnabled() {
        checkSetToolbarContainerTopMarginForAutoHiddenVerticalTab();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TOOLBAR_SNAPSHOT_REFACTOR)
    public void testSetToolbarContainerTopMarginForAutoHiddenVerticalTab_RefactorDisabled() {
        checkSetToolbarContainerTopMarginForAutoHiddenVerticalTab();
    }

    @Test
    public void testSetToolbarContainerTopMarginForAutoHiddenVerticalTab_NotInDesktopWindow() {
        initControlContainer(R.layout.toolbar_tablet);
        SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier =
                ObservableSuppliers.createNonNull(true);
        mControlContainer.setIsVerticalTabsActiveSupplier(isVerticalTabsActiveSupplier);
        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(0, 0, 100, 100), false);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);

        View toolbarContainer = mControlContainer.findViewById(R.id.toolbar_container);
        assertNotNull(toolbarContainer);

        MarginLayoutParams lp = (MarginLayoutParams) toolbarContainer.getLayoutParams();
        mControlContainer.setToolbarContainerTopMarginForAutoHiddenVerticalTab(true);
        mControlContainer.onMeasure(0, 0);
        assertEquals(0, lp.topMargin);
    }

    @Test
    public void testSetToolbarContainerTopMarginForAutoHiddenVerticalTab_InactiveVerticalTabs() {
        initControlContainer(R.layout.toolbar_tablet);
        SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier =
                ObservableSuppliers.createNonNull(false);
        mControlContainer.setIsVerticalTabsActiveSupplier(isVerticalTabsActiveSupplier);
        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, 0, 80, 100), true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);

        View toolbarContainer = mControlContainer.findViewById(R.id.toolbar_container);
        assertNotNull(toolbarContainer);

        MarginLayoutParams lp = (MarginLayoutParams) toolbarContainer.getLayoutParams();
        mControlContainer.setToolbarContainerTopMarginForAutoHiddenVerticalTab(true);
        mControlContainer.onMeasure(0, 0);
        assertEquals(0, lp.topMargin);
    }

    @Test
    public void testSetToolbarContainerTopMargin_VerticalTabs_WithCaptionControlsTopOffset() {
        initControlContainer(R.layout.toolbar_tablet);
        SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier =
                ObservableSuppliers.createNonNull(true);
        mControlContainer.setIsVerticalTabsActiveSupplier(isVerticalTabsActiveSupplier);

        int topOffset = 24;
        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, topOffset, 80, 64), true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);
        when(mToolbarDataProvider.getPrimaryColor()).thenReturn(Color.BLUE);

        View toolbarContainer = mControlContainer.findViewById(R.id.toolbar_container);
        assertNotNull(toolbarContainer);

        MarginLayoutParams lp = (MarginLayoutParams) toolbarContainer.getLayoutParams();
        mControlContainer.onAppHeaderStateChanged(appHeaderState);
        assertEquals(topOffset, lp.topMargin);
        assertEquals(
                mControlContainer
                                .getContext()
                                .getResources()
                                .getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                        + topOffset,
                mControlContainer.getToolbarHeight());
        verify(mTopControlsStacker, atLeastOnce())
                .requestLayerUpdatePost(/* requireAnimate= */ false);
        assertTrue(mControlContainer.getBackground() instanceof ColorDrawable);
        assertEquals(Color.BLUE, ((ColorDrawable) mControlContainer.getBackground()).getColor());

        int tabStripHeight =
                mControlContainer
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_strip_height);
        mControlContainer.setToolbarContainerTopMarginForAutoHiddenVerticalTab(true);
        assertEquals(topOffset + tabStripHeight, lp.topMargin);

        isVerticalTabsActiveSupplier.set(false);
        assertEquals(0, lp.topMargin);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_SNAPSHOT_REFACTOR)
    public void
            testUpdateToolbarContainerTopMargin_TabStripHeightChange_DoesNotRequestLayerUpdatePost() {
        initControlContainer(R.layout.toolbar_tablet);
        SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier =
                ObservableSuppliers.createNonNull(false);
        mControlContainer.setIsVerticalTabsActiveSupplier(isVerticalTabsActiveSupplier);

        doReturn(80).when(mToolbar).getTabStripHeight();
        mControlContainer.onMeasure(/* widthMeasureSpec= */ 0, /* heightMeasureSpec= */ 0);

        View toolbarContainer = mControlContainer.findViewById(R.id.toolbar_container);
        assertNotNull(toolbarContainer);
        MarginLayoutParams lp = (MarginLayoutParams) toolbarContainer.getLayoutParams();
        assertEquals(80, lp.topMargin);
        verify(mTopControlsStacker, never()).requestLayerUpdatePost(anyBoolean());
    }

    @Test
    public void testSystemGestureExclusionsInDesktopWindow() {
        initControlContainer(R.layout.toolbar_tablet);

        SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier =
                ObservableSuppliers.createNonNull(true);
        mControlContainer.setIsVerticalTabsActiveSupplier(isVerticalTabsActiveSupplier);

        // Layout the control container to have a non-zero width.
        mControlContainer.layout(0, 0, 200, 100);

        // Set app header with 10px padding on left, 20px on right, and 100px height.
        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, 0, 80, 100), true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);

        // Initially tab strip is visible (height 80). Passes through exclusions.
        mControlContainer.onHeightChanged(80, 20, false);
        List<Rect> inputRects = List.of(new Rect(1, 2, 3, 4));
        mControlContainer.setSystemGestureExclusionRects(inputRects);
        assertEquals(
                "System gesture exclusions should pass through when tab strip is visible.",
                inputRects,
                mControlContainer.getSystemGestureExclusionRects());

        // Suppress tab strip (height 0). Overrides with toolbar exclusion rect.
        // Left padding = 10, right padding = 20.
        // Caption controls height = 100. Top offset = 0.
        // Expected exclusion rect: Rect(10, 0, 200 - 20, 100).
        mControlContainer.onHeightChanged(0, 20, false);
        List<Rect> expected = List.of(new Rect(0, 0, 180, 100));
        assertEquals(
                "System gesture exclusions should be overridden when tab strip is hidden.",
                expected,
                mControlContainer.getSystemGestureExclusionRects());

        // Exit desktop window. Overrides should be cleared.
        var appHeaderState2 =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, 0, 80, 100), false);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState2);
        mControlContainer.onAppHeaderStateChanged(appHeaderState2);
        assertTrue(
                "System gesture exclusions should be cleared when exiting desktop window.",
                mControlContainer.getSystemGestureExclusionRects().isEmpty());
    }

    @Test
    public void testSystemGestureExclusions_WithVerticalTabsWidth() {
        initControlContainer(R.layout.toolbar_tablet);

        SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier =
                ObservableSuppliers.createNonNull(true);
        mControlContainer.setIsVerticalTabsActiveSupplier(isVerticalTabsActiveSupplier);

        // Layout the control container to have width = 500px, height = 100px.
        mControlContainer.layout(0, 0, 500, 100);

        // AppHeaderState: appHeader width = 500, unoccluded region right edge = 480 (right padding
        // = 20px).
        var appHeaderState =
                new AppHeaderState(
                        /* appWindowRect= */ new Rect(0, 0, 500, 100),
                        /* widestUnoccludedRect= */ new Rect(10, 0, 480, 100),
                        /* isInDesktopWindow= */ true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);

        // Suppress tab strip height to 0, which is the case when Vertical Tabs are enabled. This
        // also calls #updateSystemGestureExclusions.
        mControlContainer.onHeightChanged(0, 20, false);

        // Default vertical tabs width supplier (width = 0).
        List<Rect> expectedDefault = List.of(new Rect(0, 0, 480, 100));
        assertEquals(
                "Exclusion left edge should be 0 when vertical tabs width supplier is default.",
                expectedDefault,
                mControlContainer.getSystemGestureExclusionRects());

        // Supply an arbitrary vertical tabs width 150px.
        SettableNonNullObservableSupplier<Integer> widthSupplier =
                ObservableSuppliers.createNonNull(150);
        mControlContainer.setVerticalTabsContainerWidthSupplier(widthSupplier);

        // Left edge should still be 0 so toolbar buttons at top left receive clicks.
        List<Rect> expectedWithRail = List.of(new Rect(0, 0, 480, 100));
        assertEquals(
                "Exclusion left edge should remain 0 when vertical tabs are active.",
                expectedWithRail,
                mControlContainer.getSystemGestureExclusionRects());
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
    public void testTopLeftCornerOverlayPositionWithRtl() {
        initControlContainer(R.layout.toolbar_tablet);

        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, 0, 80, 100), true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);
        mControlContainer.onAppHeaderStateChanged(appHeaderState);

        SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier =
                ObservableSuppliers.createNonNull(true);
        mControlContainer.setIsVerticalTabsActiveSupplier(isVerticalTabsActiveSupplier);

        View overlayView = mControlContainer.getTopLeftCornerOverlayViewForTesting();
        assertNotNull(overlayView);
        assertEquals(View.VISIBLE, overlayView.getVisibility());

        // Test in LTR mode.
        mControlContainer.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);
        mControlContainer.layout(0, 0, 500, 100);
        assertEquals("Corner overlay should be on the left in LTR mode", 0, overlayView.getLeft());

        // Test in RTL mode.
        mControlContainer.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mControlContainer.layout(0, 0, 500, 100);
        assertEquals("Corner overlay should be on the left in RTL mode", 0, overlayView.getLeft());
    }

    @Test
    public void testTopLeftCornerOverlay_OnTabOrModelChanged() {
        initControlContainer(R.layout.toolbar_tablet);

        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, 0, 80, 100), true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);
        mControlContainer.onAppHeaderStateChanged(appHeaderState);

        SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier =
                ObservableSuppliers.createNonNull(true);
        mControlContainer.setIsVerticalTabsActiveSupplier(isVerticalTabsActiveSupplier);

        View overlayView = mControlContainer.getTopLeftCornerOverlayViewForTesting();
        assertNotNull(overlayView);
        assertEquals(View.VISIBLE, overlayView.getVisibility());

        // Ensure background is null to verify onTabOrModelChanged updates state even when
        // background is null.
        mControlContainer.setBackground(null);

        Context context = mControlContainer.getContext();
        Canvas canvas = mock(Canvas.class);
        ArgumentCaptor<Paint> paintCaptor = ArgumentCaptor.forClass(Paint.class);

        // Switch to incognito model.
        mControlContainer.onTabOrModelChanged(/* incognito= */ true);
        overlayView.draw(canvas);
        verify(canvas, atLeastOnce()).drawPath(any(), paintCaptor.capture());
        assertEquals(
                "Corner overlay should draw with incognito tab strip background color",
                TabUiThemeUtil.getTabStripBackgroundColor(context, true),
                paintCaptor.getValue().getColor());

        // Switch back to regular model.
        clearInvocations(canvas);
        mControlContainer.onTabOrModelChanged(/* incognito= */ false);
        overlayView.draw(canvas);
        verify(canvas, atLeastOnce()).drawPath(any(), paintCaptor.capture());
        assertEquals(
                "Corner overlay should draw with regular tab strip background color",
                TabUiThemeUtil.getTabStripBackgroundColor(context, false),
                paintCaptor.getValue().getColor());
    }

    @Test
    public void testShowLocationBarOnly() {
        doReturn(mLocationBarView).when(mToolbar).removeLocationBarView();
        doReturn(Color.RED).when(mToolbarDataProvider).getPrimaryColor();
        ToolbarControlContainer controlContainer =
                (ToolbarControlContainer)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.control_container, /* root= */ null);
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
                null,
                mTopControlsStacker);

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
                /* downloadButtonCoordinator= */ null,
                mThemeColorProvider,
                mIncognitoStateProvider,
                /* incognitoWindowCountSupplier= */ null,
                mWindowAndroid);

        mToolbarView.addView(mLocationBarView);
        controlContainer.toggleLocationBarOnlyMode(true);
        assertEquals(View.GONE, mProgressBar.getDesiredAndroidVisibility());
        assertEquals(View.GONE, mToolbarView.getVisibility());
        assertEquals(0, mToolbarView.getChildCount());

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
        assertEquals(View.VISIBLE, mProgressBar.getDesiredAndroidVisibility());
        assertEquals(View.VISIBLE, mToolbarView.getVisibility());
        verify(mToolbar).restoreLocationBarView();
        assertEquals(
                Color.TRANSPARENT, ((ColorDrawable) controlContainer.getBackground()).getColor());
    }

    @Test
    public void testInterceptTouchEvent() {
        ToolbarControlContainer controlContainer =
                (ToolbarControlContainer)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.control_container, /* root= */ null);
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
                null,
                mTopControlsStacker);
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
    public void testTouchEvent_BelowToolbarContainer() {
        ToolbarControlContainer controlContainer =
                (ToolbarControlContainer)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.control_container, /* root= */ null);
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
                null,
                mTopControlsStacker);
        ToolbarControlContainer.ToolbarViewResourceCoordinatorLayout toolbarContainer =
                controlContainer.findViewById(R.id.toolbar_container);
        toolbarContainer.setVisibility(View.VISIBLE);
        toolbarContainer.layout(0, 0, 1000, 100);
        controlContainer.setSwipeHandler(mock(SwipeHandler.class));

        // Click within the toolbar container.
        MotionEvent toolbarClickEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 50, 0);
        assertFalse(controlContainer.onInterceptTouchEvent(toolbarClickEvent));
        assertTrue(controlContainer.onTouchEvent(toolbarClickEvent));

        // Click below the toolbar container when tablet find in page is open.
        ViewStub findToolbarStub = controlContainer.findViewById(R.id.find_toolbar_tablet_stub);
        View findToolbar = findToolbarStub.inflate();
        findToolbar.setVisibility(View.VISIBLE);
        findToolbar.layout(600, 100, 1000, 200);

        MotionEvent belowToolbarClickEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 150, 0);
        assertFalse(controlContainer.onInterceptTouchEvent(belowToolbarClickEvent));
        assertFalse(controlContainer.onTouchEvent(belowToolbarClickEvent));
    }

    @Test
    public void testTouchEvent_BelowToolbarContainer_StackedChild_GapPassesThrough() {
        initControlContainerForBelowToolbarTouch();
        View stackedChild = addStackedChild();
        stackedChild.layout(0, 140, 1000, 180);

        // Click in the gap between the toolbar container and the stacked child (e.g. where the
        // bookmark bar is).
        MotionEvent gapClickEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 120, 0);
        assertFalse(mControlContainer.onInterceptTouchEvent(gapClickEvent));
        assertFalse(mControlContainer.onTouchEvent(gapClickEvent));
    }

    @Test
    public void testTouchEvent_BelowToolbarContainer_StackedChild_TouchOnChildConsumed() {
        initControlContainerForBelowToolbarTouch();
        View stackedChild = addStackedChild();
        stackedChild.layout(0, 140, 1000, 180);

        // A click on the stacked child's background should not fall through.
        MotionEvent childClickEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 160, 0);
        assertTrue(mControlContainer.onTouchEvent(childClickEvent));
    }

    @Test
    public void testTouchEvent_BelowToolbarContainer_StackedChild_ButtonReceivesTouch() {
        initControlContainerForBelowToolbarTouch();
        FrameLayout stackedChild = addStackedChild();
        View button = new View(mActivity);
        button.setClickable(true);
        OnTouchListener touchListener = mock(OnTouchListener.class);
        doReturn(true).when(touchListener).onTouch(eq(button), any(MotionEvent.class));
        button.setOnTouchListener(touchListener);
        stackedChild.addView(button);
        stackedChild.layout(0, 140, 1000, 180);
        button.layout(0, 0, 200, 40);

        MotionEvent buttonClickEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 160, 0);
        assertTrue(mControlContainer.dispatchTouchEvent(buttonClickEvent));
        ArgumentCaptor<MotionEvent> eventCaptor = ArgumentCaptor.forClass(MotionEvent.class);
        verify(touchListener).onTouch(eq(button), eventCaptor.capture());
        assertEquals(MotionEvent.ACTION_DOWN, eventCaptor.getValue().getActionMasked());
    }

    @Test
    public void testTouchEvent_BelowToolbarContainer_StackedChild_GapReachesSiblingBelow() {
        initControlContainerForBelowToolbarTouch();
        View stackedChild = addStackedChild();

        // The bookmark bar is a sibling drawn below the control container. The control container
        // is added last so that it is hit-tested first. It was inflated without a parent, so it can
        // be added to a new one.
        assertNull(mControlContainer.getParent());
        FrameLayout parent = new FrameLayout(mActivity);
        View bookmarkSibling = new View(mActivity);
        OnTouchListener siblingTouchListener = mock(OnTouchListener.class);
        doReturn(true)
                .when(siblingTouchListener)
                .onTouch(eq(bookmarkSibling), any(MotionEvent.class));
        bookmarkSibling.setOnTouchListener(siblingTouchListener);
        parent.addView(bookmarkSibling);
        parent.addView(mControlContainer);

        // Lay out top-down; each layout() call re-lays out the children of the laid out view.
        parent.layout(0, 0, 1000, 1000);
        bookmarkSibling.layout(0, 0, 1000, 1000);
        mControlContainer.layout(0, 0, 1000, 180);
        mControlContainer.findViewById(R.id.toolbar_container).layout(0, 0, 1000, 100);
        stackedChild.layout(0, 140, 1000, 180);

        MotionEvent gapClickEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 120, 0);
        assertTrue(parent.dispatchTouchEvent(gapClickEvent));
        ArgumentCaptor<MotionEvent> eventCaptor = ArgumentCaptor.forClass(MotionEvent.class);
        verify(siblingTouchListener).onTouch(eq(bookmarkSibling), eventCaptor.capture());
        assertEquals(MotionEvent.ACTION_DOWN, eventCaptor.getValue().getActionMasked());
    }

    @Test
    public void testTouchEvent_BelowToolbarContainer_NoStackedChild_Unchanged() {
        initControlContainerForBelowToolbarTouch();

        MotionEvent belowToolbarClickEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 150, 0);
        assertTrue(mControlContainer.onTouchEvent(belowToolbarClickEvent));
    }

    @Test
    public void testTouchEvent_BelowToolbarContainer_StackedChildGone_Unchanged() {
        initControlContainerForBelowToolbarTouch();
        View stackedChild = addStackedChild();
        stackedChild.layout(0, 140, 1000, 180);
        stackedChild.setVisibility(View.GONE);

        MotionEvent gapClickEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 120, 0);
        assertTrue(mControlContainer.onTouchEvent(gapClickEvent));
    }

    @Test
    public void testInterceptTouchEvent_BelowToolbarContainer_OnFindToolbar_NotIntercepted() {
        SwipeHandler swipeHandler = initControlContainerForBelowToolbarTouch();
        mControlContainer.addTouchEventObserver(mTouchEventObserver);
        ViewStub findToolbarStub = mControlContainer.findViewById(R.id.find_toolbar_tablet_stub);
        View findToolbar = findToolbarStub.inflate();
        findToolbar.setVisibility(View.VISIBLE);
        findToolbar.layout(600, 100, 1000, 180);

        // A horizontal drag inside the find toolbar (e.g. in its text field) must not be treated
        // as a toolbar swipe.
        assertHorizontalDragNotIntercepted(/* y= */ 150);
        verifyNoInteractions(swipeHandler);
        verify(mTouchEventObserver, never()).onInterceptTouchEvent(any(MotionEvent.class));
    }

    @Test
    public void testInterceptTouchEvent_BelowToolbarContainer_OnStackedChild_NotIntercepted() {
        SwipeHandler swipeHandler = initControlContainerForBelowToolbarTouch();
        mControlContainer.addTouchEventObserver(mTouchEventObserver);
        View stackedChild = addStackedChild();
        stackedChild.layout(0, 140, 1000, 180);

        // A horizontal drag on the stacked child (e.g. the tab sharing toolbar) must not be
        // treated as a toolbar swipe.
        assertHorizontalDragNotIntercepted(/* y= */ 160);
        verifyNoInteractions(swipeHandler);
        verify(mTouchEventObserver, never()).onInterceptTouchEvent(any(MotionEvent.class));
    }

    @Test
    public void testTouchEvent_BelowToolbarContainer_FindToolbarBackgroundConsumed() {
        initControlContainerForBelowToolbarTouch();
        ViewStub findToolbarStub = mControlContainer.findViewById(R.id.find_toolbar_tablet_stub);
        View findToolbar = findToolbarStub.inflate();
        findToolbar.setVisibility(View.VISIBLE);
        findToolbar.layout(600, 100, 1000, 180);

        // A tap on the find toolbar's background that no child claims is consumed rather than
        // falling through to views below.
        MotionEvent findToolbarClickEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 700, 150, 0);
        assertTrue(mControlContainer.onTouchEvent(findToolbarClickEvent));
    }

    @Test
    public void testOnTouchEvent() {
        ToolbarControlContainer controlContainer =
                (ToolbarControlContainer)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.control_container, /* root= */ null);
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
                null,
                mTopControlsStacker);

        SwipeHandler swipeHandler = mock(SwipeHandler.class);
        controlContainer.setSwipeHandler(swipeHandler);
        doReturn(100).when(mToolbar).getTabStripHeight();

        // ACTION_DOWN on the tab strip (y <= 100) should return false so that the tab strip
        // handles it.
        MotionEvent downTabStripEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 50, 0);
        assertFalse(controlContainer.onTouchEvent(downTabStripEvent));

        // ACTION_DOWN on the toolbar (y > 100) should return true so the toolbar captures the
        // gesture.
        MotionEvent downToolbarEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 150, 0);
        assertTrue(controlContainer.onTouchEvent(downToolbarEvent));

        // ACTION_MOVE on the tab strip during an ongoing toolbar gesture should not return false
        // early, but be delegated to the swipe gesture listener.
        MotionEvent moveTabStripEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 50, 50, 0);
        controlContainer.onTouchEvent(moveTabStripEvent);
    }

    @Test
    public void testHeightSupplier() {
        var controlContainer = new ToolbarControlContainer(mActivity, /* attrs= */ null);
        var heightSupplier = ObservableSuppliers.createNonNull(-1);
        controlContainer.setOnHeightChangedListener(heightSupplier);
        controlContainer.onSizeChanged(100, 200, 100, 100);
        assertEquals(200, (int) heightSupplier.get());
    }

    @Test
    public void testHeightSupplier_noHeightChange() {
        var controlContainer = new ToolbarControlContainer(mActivity, /* attrs= */ null);
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
        mToolbarHairline.layout(0, 0, 1, hairlineHeight);

        // Start transition
        mControlContainer.onHeightChanged(tabStripHeight, 0, true);

        assertEquals(
                "Toolbar top margin is wrong.",
                tabStripHeight,
                ((MarginLayoutParams) mToolbarView.getLayoutParams()).topMargin);
        assertEquals(
                "Hairline top margin is wrong.",
                tabStripHeight + toolbarHeight,
                ((MarginLayoutParams) mToolbarHairline.getLayoutParams()).topMargin);

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
        mToolbarHairline.layout(0, 0, 1, hairlineHeight);
        ((MarginLayoutParams) mToolbarView.getLayoutParams()).topMargin = tabStripHeight;

        mControlContainer.onHeightChanged(0, 0, true);

        assertEquals(
                "Toolbar top margin is wrong.",
                0,
                ((MarginLayoutParams) mToolbarView.getLayoutParams()).topMargin);
        assertEquals(
                "Hairline top margin is wrong.",
                toolbarHeight,
                ((MarginLayoutParams) mToolbarHairline.getLayoutParams()).topMargin);

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
        mToolbarHairline.layout(0, 0, 1, hairlineHeight);
        ((MarginLayoutParams) mToolbarView.getLayoutParams()).topMargin = tabStripHeight;

        mControlContainer.onHeightChanged(0, 0, true);

        assertEquals(
                "Toolbar top margin is wrong.",
                0,
                ((MarginLayoutParams) mToolbarView.getLayoutParams()).topMargin);
        assertEquals(
                "Hairline top margin is wrong.",
                toolbarHeight,
                ((MarginLayoutParams) mToolbarHairline.getLayoutParams()).topMargin);

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
        mControlContainer.findViewById(R.id.toolbar_container).setVisibility(View.VISIBLE);
        mControlContainer.findViewById(R.id.toolbar).setVisibility(View.GONE);
        mToolbarHairline.setMinimumHeight(10);
        mControlContainer.layout(0, 0, 100, 0);
        when(mToolbarDataProvider.getUrlBarData()).thenReturn(UrlBarData.EMPTY);
        AtomicInteger captureCount = new AtomicInteger();
        mControlContainer
                .getToolbarResourceAdapter()
                .addOnResourceReadyCallback(resource -> captureCount.incrementAndGet());

        // Test with forceCaptureAfterLayout = false
        mControlContainer.doSynchronousLayout(false);
        assertTrue(mControlContainer.getMeasuredHeight() > 0);
        assertEquals(mControlContainer.getMeasuredHeight(), mControlContainer.getHeight());
        assertEquals(0, captureCount.get());

        // Test with forceCaptureAfterLayout = true
        mControlContainer.doSynchronousLayout(true);
        assertEquals(1, captureCount.get());

        // Test with explicit positive height (e.g. mini origin bar).
        int explicitHeight = 50;
        mControlContainer.setLayoutParams(
                new CoordinatorLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, explicitHeight));
        mControlContainer.doSynchronousLayout(false);
        assertEquals(explicitHeight, mControlContainer.getMeasuredHeight());
        assertEquals(explicitHeight, mControlContainer.getHeight());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
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
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
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
        doReturn(true).when(buttonData).isIdentityDisc();
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

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_SNAPSHOT_REFACTOR)
    public void testOnMeasure_BottomAnchoredToolbarHairlineTopMarginZero() {
        initControlContainer(R.layout.toolbar_phone);

        Resources res = mActivity.getResources();
        int toolbarLayoutHeight = res.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);

        CoordinatorLayout.LayoutParams hairlineParams =
                mControlContainer.mutateHairlineLayoutParams();
        hairlineParams.anchorGravity = Gravity.TOP;
        hairlineParams.topMargin = toolbarLayoutHeight;

        int widthSpec = View.MeasureSpec.makeMeasureSpec(1024, View.MeasureSpec.EXACTLY);
        int heightSpec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
        mControlContainer.measure(widthSpec, heightSpec);

        assertEquals(
                "Bottom-anchored toolbar hairline top margin should be 0.",
                0,
                hairlineParams.topMargin);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_SNAPSHOT_REFACTOR)
    public void
            testOnHeightChanged_WithToolbarSnapshotRefactorEnabled_SetsToolbarViewTopMarginToZero() {
        initControlContainer(R.layout.toolbar_tablet);
        int toolbarLayoutHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        ((MarginLayoutParams) mToolbarView.getLayoutParams()).topMargin = 80;
        mControlContainer.onHeightChanged(
                /* tabStripHeight= */ 80, /* topPadding= */ 20, /* applyScrimOverlay= */ false);

        assertEquals(0, ((MarginLayoutParams) mToolbarView.getLayoutParams()).topMargin);
        assertEquals(
                toolbarLayoutHeight,
                ((MarginLayoutParams) mToolbarHairline.getLayoutParams()).topMargin);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TOOLBAR_SNAPSHOT_REFACTOR)
    public void
            testOnHeightChanged_WithToolbarSnapshotRefactorDisabled_SetsToolbarViewTopMarginToTabStripHeight() {
        initControlContainer(R.layout.toolbar_tablet);
        int toolbarLayoutHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        mControlContainer.onHeightChanged(
                /* tabStripHeight= */ 80, /* topPadding= */ 20, /* applyScrimOverlay= */ false);

        assertEquals(80, ((MarginLayoutParams) mToolbarView.getLayoutParams()).topMargin);
        assertEquals(
                80 + toolbarLayoutHeight,
                ((MarginLayoutParams) mToolbarHairline.getLayoutParams()).topMargin);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TOOLBAR_SNAPSHOT_REFACTOR)
    public void
            testOnHeightChanged_VerticalTabsWithCaptionControlsTopOffset_ExcludesTopOffsetFromHairlineTopMargin() {
        checkOnHeightChangedWithVerticalTabsCaptionControlsTopOffset(
                /* expectTopOffsetExcluded= */ true);
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.TOOLBAR_SNAPSHOT_REFACTOR,
        ChromeFeatureList.FIX_TOOLBAR_HAIRLINE_REGRESSION
    })
    public void
            testOnHeightChanged_VerticalTabsWithCaptionControlsTopOffset_WithFixDisabled_IncludesTopOffsetInHairlineTopMargin() {
        checkOnHeightChangedWithVerticalTabsCaptionControlsTopOffset(
                /* expectTopOffsetExcluded= */ false);
    }

    private void checkSetToolbarContainerTopMarginForAutoHiddenVerticalTab() {
        initControlContainer(R.layout.toolbar_tablet);
        SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier =
                ObservableSuppliers.createNonNull(true);
        mControlContainer.setIsVerticalTabsActiveSupplier(isVerticalTabsActiveSupplier);
        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, 0, 80, 100), true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);

        View toolbarContainer = mControlContainer.findViewById(R.id.toolbar_container);
        assertNotNull(toolbarContainer);

        MarginLayoutParams lp = (MarginLayoutParams) toolbarContainer.getLayoutParams();
        int tabStripHeight =
                mControlContainer
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_strip_height);

        mControlContainer.setToolbarContainerTopMarginForAutoHiddenVerticalTab(true);
        mControlContainer.onMeasure(/* widthMeasureSpec= */ 0, /* heightMeasureSpec= */ 0);
        assertEquals(tabStripHeight, lp.topMargin);

        mControlContainer.setToolbarContainerTopMarginForAutoHiddenVerticalTab(false);
        mControlContainer.onMeasure(/* widthMeasureSpec= */ 0, /* heightMeasureSpec= */ 0);
        assertEquals(0, lp.topMargin);
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

    private void checkOnHeightChangedWithVerticalTabsCaptionControlsTopOffset(
            boolean expectTopOffsetExcluded) {
        initControlContainer(R.layout.toolbar_tablet);
        Resources res = mActivity.getResources();
        int toolbarLayoutHeight = res.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        int hairlineHeight = res.getDimensionPixelSize(R.dimen.toolbar_hairline_height);
        mToolbarHairline.layout(0, 0, 1, hairlineHeight);

        SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier =
                ObservableSuppliers.createNonNull(true);
        mControlContainer.setIsVerticalTabsActiveSupplier(isVerticalTabsActiveSupplier);

        int topOffset = 24;
        var appHeaderState =
                new AppHeaderState(
                        /* appWindowRect= */ new Rect(0, 0, 100, 100),
                        /* widestUnoccludedRect= */ new Rect(10, topOffset, 80, 64),
                        /* isInDesktopWindow= */ true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);
        mControlContainer.onAppHeaderStateChanged(appHeaderState);

        mControlContainer.onHeightChanged(
                /* tabStripHeight= */ 0, /* topPadding= */ 0, /* applyScrimOverlay= */ false);

        assertEquals(0, ((MarginLayoutParams) mToolbarView.getLayoutParams()).topMargin);
        assertEquals(
                expectTopOffsetExcluded ? toolbarLayoutHeight : toolbarLayoutHeight + topOffset,
                ((MarginLayoutParams) mToolbarHairline.getLayoutParams()).topMargin);
        assertEquals(
                toolbarLayoutHeight + topOffset + hairlineHeight,
                mControlContainer.getMinimumHeight());
    }

    /** Stubs the toolbar's reported position within its container. */
    private void setToolbarPositionInContainer(int y) {
        doAnswer(
                        invocation -> {
                            int[] position = invocation.getArgument(1);
                            position[0] = 0;
                            position[1] = y;
                            return null;
                        })
                .when(mToolbar)
                .getPositionRelativeToContainer(any(), any());
    }

    @Test
    public void testCreateNativeResource_RecordsToolbarTopOffset() {
        initControlContainer(R.layout.toolbar_tablet);
        ToolbarViewResourceAdapter adapter =
                (ToolbarViewResourceAdapter) mControlContainer.getToolbarResourceAdapter();
        setToolbarPositionInContainer(40);

        adapter.createNativeResource();

        assertEquals(40, mControlContainer.getToolbarTopOffsetInCapture());
    }

    @Test
    public void testToolbarTopOffset_NotUpdatedUntilNextCapture() {
        // The offset must describe the bitmap the compositor is holding, not the live view, so
        // that it stays consistent with a stale capture. Moving the toolbar without capturing
        // must not change it.
        initControlContainer(R.layout.toolbar_tablet);
        ToolbarViewResourceAdapter adapter =
                (ToolbarViewResourceAdapter) mControlContainer.getToolbarResourceAdapter();
        setToolbarPositionInContainer(40);
        adapter.createNativeResource();
        assertEquals(40, mControlContainer.getToolbarTopOffsetInCapture());

        // The tab strip goes away, moving the toolbar to the top of the container, but no new
        // capture has been taken yet.
        setToolbarPositionInContainer(0);

        assertEquals(
                "Offset must still describe the previously captured bitmap.",
                40,
                mControlContainer.getToolbarTopOffsetInCapture());

        // Once a new capture happens, it catches up.
        adapter.createNativeResource();

        assertEquals(0, mControlContainer.getToolbarTopOffsetInCapture());
    }

    @Test
    public void testCreateNativeResource_IncludesEdgeToEdgeTopPadding() {
        // When drawing edge-to-edge (e.g. a themed NTP), the toolbar is padded at the top to draw
        // behind the status bar. That padding sits above the toolbar's content in the capture.
        initControlContainer(R.layout.toolbar_tablet);
        ToolbarViewResourceAdapter adapter =
                (ToolbarViewResourceAdapter) mControlContainer.getToolbarResourceAdapter();
        setToolbarPositionInContainer(0);
        when(mToolbar.getEdgeToEdgeTopPadding()).thenReturn(60);

        adapter.createNativeResource();

        assertEquals(60, mControlContainer.getToolbarTopOffsetInCapture());
    }

    @Test
    public void testToolbarTopOffset_EdgeToEdgePaddingRemovedBeforeNextCapture() {
        // Regression test for https://crbug.com/565656031: navigating away from an edge-to-edge
        // NTP removes the toolbar's top padding, but the capture isn't retaken right away. The
        // offset must keep accounting for the padding still present in the stale bitmap.
        initControlContainer(R.layout.toolbar_tablet);
        ToolbarViewResourceAdapter adapter =
                (ToolbarViewResourceAdapter) mControlContainer.getToolbarResourceAdapter();
        setToolbarPositionInContainer(0);
        when(mToolbar.getEdgeToEdgeTopPadding()).thenReturn(60);
        adapter.createNativeResource();
        assertEquals(60, mControlContainer.getToolbarTopOffsetInCapture());

        when(mToolbar.getEdgeToEdgeTopPadding()).thenReturn(0);

        assertEquals(
                "Offset must still describe the previously captured bitmap.",
                60,
                mControlContainer.getToolbarTopOffsetInCapture());

        adapter.createNativeResource();

        assertEquals(0, mControlContainer.getToolbarTopOffsetInCapture());
    }
}
