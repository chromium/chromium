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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
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
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.ToolbarHairlineView;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.back_button.BackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.forward_button.ForwardButtonCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.reload_button.ReloadButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarAllowCaptureReason;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer.ToolbarViewResourceAdapter;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer.ToolbarViewResourceAdapter.ToolbarInMotionStage;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer.ToolbarViewResourceCoordinatorLayout;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.ui.base.TestActivity;
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
    @Mock private HomeButtonDisplay mHomeButtonDisplay;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;
    @Mock private NewTabPageDelegate mNewTabPageDelegate;
    @Captor private ArgumentCaptor<CoordinatorLayout.LayoutParams> mToolbarLayoutParamsCaptor;
    @Captor private ArgumentCaptor<CoordinatorLayout.LayoutParams> mHairlineLayoutParamsCaptor;

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
                mToolbarDataProvider);
        // The adapter may observe some of these already, which will post events.
        ShadowLooper.idleMainLooper();
        // The initial addObserver triggers an event that we don't care about. Reset count.
        mOnResourceRequestedCount.set(0);
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
                mToolbarDataProvider);
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
        ResourceFactoryJni.setInstanceForTesting(mResourceFactoryJni);
        when(mToolbarContainer.getWidth()).thenReturn(1);
        when(mToolbarContainer.getHeight()).thenReturn(1);
        when(mToolbarContainer.findViewById(anyInt())).thenReturn(mToolbarHairline);
        when(mToolbarHairline.getHeight()).thenReturn(1);
        doReturn(mProgressBar).when(mToolbar).getProgressBar();
        doReturn(new CoordinatorLayout.LayoutParams(-1, -1)).when(mToolbarView).getLayoutParams();
        doReturn(new CoordinatorLayout.LayoutParams(-1, -1))
                .when(mToolbarHairline)
                .getLayoutParams();
        mBrowserStateBrowserControlsVisibilityDelegate.set(BrowserControlsState.BOTH);
        mCompositorInMotionSupplier.set(false);
        mBrowserStateBrowserControlsVisibilityDelegate.addObserver(
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
    @DisableFeatures(ChromeFeatureList.TOOLBAR_STALE_CAPTURE_BUG_FIX)
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

    @Test
    @DisableFeatures(ChromeFeatureList.TOOLBAR_STALE_CAPTURE_BUG_FIX)
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
        ToolbarControlContainer controlContainer = new ToolbarControlContainer(mActivity, null);
        // This is needed for the control container to read the height of the toolbar.
        controlContainer.setToolbarForTesting(mToolbar);

        // Set app header with 10px padding on left, 20px on right, and 100px height. Set tab strip
        // height to 80px. Top inset should be 100 - 80 = 20.
        doReturn(80).when(mToolbar).getTabStripHeight();
        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, 0, 80, 100), true);
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
        assertEquals(
                "Top inset for tab drawable is wrong.",
                20,
                background.getLayerInsetTop(tabDrawableIndex));

        // Set app header with 40px height, and tab strip with 50px height.
        // Top inset should be max(0, 40 - 50) = 0.
        appHeaderState = new AppHeaderState(new Rect(0, 0, 100, 40), new Rect(10, 0, 80, 40), true);
        controlContainer.onAppHeaderStateChanged(appHeaderState);
        background = (LayerDrawable) controlContainer.getBackground();
        assertEquals(
                "Top inset for tab drawable should be 0.",
                0,
                background.getLayerInsetTop(tabDrawableIndex));

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
        assertEquals(
                "Top inset for tab drawable should be 0.",
                0,
                background.getLayerInsetTop(tabDrawableIndex));
    }

    @Test
    public void testTempDrawableAfterCompositorInitialized() {
        ToolbarControlContainer controlContainer = new ToolbarControlContainer(mActivity, null);
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
    }

    @Test
    public void testTempDrawableInUnfocusedDesktopWindow() {
        ToolbarControlContainer controlContainer = new ToolbarControlContainer(mActivity, null);
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
                mToolbarDataProvider);

        ToolbarPhone toolbarPhone = controlContainer.findViewById(R.id.toolbar);
        doReturn(mLocationBarCoordinatorPhone).when(mLocationBarCoordinator).getPhoneCoordinator();
        doReturn(new ObservableSupplierImpl<>(AutocompleteRequestType.SEARCH))
                .when(mLocationBarCoordinator)
                .getAutocompleteRequestTypeSupplier();
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
                mHomeButtonDisplay,
                /* extensionToolbarCoordinator= */ null,
                mThemeColorProvider,
                mIncognitoStateProvider,
                /* incognitoWindowCountSupplier= */ null);

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
                mToolbarDataProvider);
        ToolbarControlContainer.ToolbarViewResourceCoordinatorLayout toolbarContainer =
                controlContainer.findViewById(R.id.toolbar_container);
        toolbarContainer.setVisibility(View.GONE);

        MotionEvent clickEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);

        assertTrue(controlContainer.onInterceptTouchEvent(clickEvent));

        toolbarContainer.setVisibility(View.VISIBLE);
        doReturn(100).when(mToolbar).getTabStripHeight();
        assertFalse(controlContainer.onInterceptTouchEvent(clickEvent));

        doReturn(0).when(mToolbar).getTabStripHeight();
        controlContainer.addTouchEventObserver(mTouchEventObserver);
        assertFalse(controlContainer.onInterceptTouchEvent(clickEvent));

        doReturn(true).when(mTouchEventObserver).onInterceptTouchEvent(clickEvent);
        assertTrue(controlContainer.onInterceptTouchEvent(clickEvent));
    }

    @Test
    public void testHeightSupplier() {
        var controlContainer = new ToolbarControlContainer(mActivity, null);
        ObservableSupplierImpl<Integer> heightSupplier = new ObservableSupplierImpl<>();
        controlContainer.setOnHeightChangedListener(heightSupplier);
        controlContainer.onSizeChanged(100, 200, 100, 100);
        assertEquals(200, (int) heightSupplier.get());
    }

    @Test
    public void testHeightSupplier_noHeightChange() {
        var controlContainer = new ToolbarControlContainer(mActivity, null);
        ObservableSupplierImpl<Integer> heightSupplier = new ObservableSupplierImpl<>();
        controlContainer.setOnHeightChangedListener(heightSupplier);
        controlContainer.onSizeChanged(100, 100, 100, 100);
        assertEquals(null, heightSupplier.get());
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
        mControlContainer.onHeightChanged(tabStripHeight, true);

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
        ShadowLooper.idleMainLooper();

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

        mControlContainer.onHeightChanged(0, true);

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
        ShadowLooper.idleMainLooper();
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

        mControlContainer.onHeightChanged(0, true);

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
        ShadowLooper.idleMainLooper();
        assertEquals(
                "Transition not finished, so minHeight stays the same.",
                0,
                mControlContainer.getMinimumHeight());
    }
}
