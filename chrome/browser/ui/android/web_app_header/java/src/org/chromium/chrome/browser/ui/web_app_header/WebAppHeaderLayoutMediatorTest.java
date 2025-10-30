// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.graphics.Rect;
import android.os.Build;
import android.os.Looper;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.web_app_header.WebAppHeaderUtils.BackEvent;
import org.chromium.chrome.browser.ui.web_app_header.WebAppHeaderUtils.ReloadType;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
@Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
public class WebAppHeaderLayoutMediatorTest {
    private static final int SCREEN_WIDTH = 800;
    private static final int SCREEN_HEIGHT = 1600;
    private static final int SYS_APP_HEADER_HEIGHT = 40;
    private static final int HEADER_BUTTON_HEIGHT = 46;
    private static final int LEFT_INSET = 50;
    private static final int RIGHT_INSET = 60;
    private static final Rect EMPTY_NON_DRAGGABLE_AREA = new Rect(0, 0, 0, 0);
    private static final Rect WIDEST_UNOCCLUDED_RECT =
            new Rect(LEFT_INSET, 0, SCREEN_WIDTH - RIGHT_INSET, SYS_APP_HEADER_HEIGHT);
    private static final int LIGHT_COLOR = 0xfffff;
    private static final int DARK_COLOR = 0x000000;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    private WebAppHeaderLayoutMediator mMediator;
    private PropertyModel mModel;
    private ObservableSupplierImpl<Tab> mTabSupplier;
    private ObservableSupplierImpl<List<Rect>> mHeaderControlPositionSupplier;
    @Mock public DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock public ThemeColorProvider mThemeColorProvider;
    @Mock public ScrimManager mScrimManager;
    @Mock public WebAppHeaderDelegate mHeaderDelegate;
    @Mock public Tab mTab;
    @Mock public WebContents mWebContents;
    @Mock public Callback<Boolean> mSetHeaderAsOverlayCallback;
    private ObservableSupplierImpl<Boolean> mScrimVisibilitySupplier;
    private @Nullable AppHeaderState mAppHeaderState;
    private ShadowLooper mShadowLooper;

    @Before
    public void setup() {
        mShadowLooper = shadowOf(Looper.getMainLooper());
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(null);
        when(mThemeColorProvider.getThemeColor()).thenReturn(LIGHT_COLOR);

        mScrimVisibilitySupplier = new ObservableSupplierImpl<>();
        when(mScrimManager.getScrimVisibilitySupplier()).thenReturn(mScrimVisibilitySupplier);

        when(mTab.getWebContents()).thenReturn(mWebContents);

        mTabSupplier = new ObservableSupplierImpl<>(mTab);
        mHeaderControlPositionSupplier = new ObservableSupplierImpl<>();
        mModel = new PropertyModel.Builder(WebAppHeaderLayoutProperties.ALL_KEYS).build();
        mMediator =
                new WebAppHeaderLayoutMediator(
                        mModel,
                        mHeaderDelegate,
                        mDesktopWindowStateManager,
                        mScrimManager,
                        mTabSupplier,
                        mHeaderControlPositionSupplier,
                        mThemeColorProvider,
                        SYS_APP_HEADER_HEIGHT,
                        HEADER_BUTTON_HEIGHT,
                        DisplayMode.MINIMAL_UI,
                        mSetHeaderAsOverlayCallback,
                        "Package name");

        mShadowLooper.idle();
    }

    private void setupDesktopWindowing(boolean isInDesktopWindow, Rect widestUnoccludedRect) {
        mAppHeaderState =
                new AppHeaderState(
                        new Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT),
                        widestUnoccludedRect,
                        isInDesktopWindow);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(mAppHeaderState);
    }

    @Test
    public void testInitialization() {
        verify(mDesktopWindowStateManager).addObserver(mMediator);
        verify(mThemeColorProvider).addThemeColorObserver(mMediator);
    }

    @Test
    public void testButtonBottomInset_headerHeightEqualButtonHeight() {
        Rect widestUnoccludedRect =
                new Rect(LEFT_INSET, 0, SCREEN_WIDTH - RIGHT_INSET, HEADER_BUTTON_HEIGHT);
        setupDesktopWindowing(/* isInDesktopWindow= */ true, widestUnoccludedRect);
        mMediator.onAppHeaderStateChanged(mAppHeaderState);

        assertEquals(
                "Button bottom inset should be zero.",
                0,
                mMediator.getButtonBottomInsetForTesting());
    }

    @Test
    public void testButtonBottomInset_headerHeightLessThanButtonHeight() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);
        mMediator.onAppHeaderStateChanged(mAppHeaderState);

        assertEquals(
                "Button bottom inset should cover the extra height needed to fit the button.",
                HEADER_BUTTON_HEIGHT - SYS_APP_HEADER_HEIGHT,
                mMediator.getButtonBottomInsetForTesting());
    }

    @Test
    public void testButtonBottomInset_adjustedHeaderHeightLessThanButtonHeight() {
        final int statusBarHeight = HEADER_BUTTON_HEIGHT - SYS_APP_HEADER_HEIGHT;

        // The caption bar is large enough to fit the buttons, but part of the caption bar is
        // occupied by the status bar.
        final Rect widestUnoccludedRect =
                new Rect(
                        LEFT_INSET,
                        statusBarHeight,
                        SCREEN_WIDTH - RIGHT_INSET,
                        HEADER_BUTTON_HEIGHT);

        setupDesktopWindowing(/* isInDesktopWindow= */ true, widestUnoccludedRect);
        mMediator.onAppHeaderStateChanged(mAppHeaderState);

        assertEquals(
                "Button bottom inset should cover the extra height needed to fit the button.",
                statusBarHeight,
                mMediator.getButtonBottomInsetForTesting());
    }

    @Test
    public void testHasAppHeaderStateOnInit_setPaddingsMatchingInsets() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);
        mMediator =
                new WebAppHeaderLayoutMediator(
                        mModel,
                        mHeaderDelegate,
                        mDesktopWindowStateManager,
                        mScrimManager,
                        mTabSupplier,
                        mHeaderControlPositionSupplier,
                        mThemeColorProvider,
                        SYS_APP_HEADER_HEIGHT,
                        HEADER_BUTTON_HEIGHT,
                        DisplayMode.MINIMAL_UI,
                        mSetHeaderAsOverlayCallback,
                        "Package name");

        assertEquals(
                "Header min height should match app header height",
                SYS_APP_HEADER_HEIGHT,
                mModel.get(WebAppHeaderLayoutProperties.MIN_HEIGHT));
        assertEquals(
                "Header paddings should match system insets",
                new Rect(LEFT_INSET, 0, RIGHT_INSET, 0),
                mModel.get(WebAppHeaderLayoutProperties.PADDINGS));
        assertTrue(
                "Header view should be visible",
                mModel.get(WebAppHeaderLayoutProperties.IS_VISIBLE));
    }

    @Test
    public void testAppHeaderStateUpdated_setPaddingsMatchingInsets() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);

        mMediator.onAppHeaderStateChanged(mAppHeaderState);
        assertEquals(
                "Header min height should match app header height",
                SYS_APP_HEADER_HEIGHT,
                mModel.get(WebAppHeaderLayoutProperties.MIN_HEIGHT));
        assertEquals(
                "Header paddings should match updated system insets",
                new Rect(LEFT_INSET, 0, RIGHT_INSET, 0),
                mModel.get(WebAppHeaderLayoutProperties.PADDINGS));
        assertTrue(
                "Header view should be visible",
                mModel.get(WebAppHeaderLayoutProperties.IS_VISIBLE));
    }

    @Test
    public void testAppHeaderStateUpdated_setNewPaddingsMatchingInsets() {
        // initial state with empty paddings
        setupDesktopWindowing(
                /* isInDesktopWindow= */ true, new Rect(0, 0, SCREEN_WIDTH, SYS_APP_HEADER_HEIGHT));
        mMediator =
                new WebAppHeaderLayoutMediator(
                        mModel,
                        mHeaderDelegate,
                        mDesktopWindowStateManager,
                        mScrimManager,
                        mTabSupplier,
                        mHeaderControlPositionSupplier,
                        mThemeColorProvider,
                        SYS_APP_HEADER_HEIGHT,
                        HEADER_BUTTON_HEIGHT,
                        DisplayMode.MINIMAL_UI,
                        mSetHeaderAsOverlayCallback,
                        "Package name");
        assertEquals(
                "Header paddings should match updated system insets",
                new Rect(0, 0, 0, 0),
                mModel.get(WebAppHeaderLayoutProperties.PADDINGS));

        // second update with updated caption paddings
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);
        mMediator.onAppHeaderStateChanged(mAppHeaderState);

        assertEquals(
                "Header paddings should match updated system insets",
                new Rect(LEFT_INSET, 0, RIGHT_INSET, 0),
                mModel.get(WebAppHeaderLayoutProperties.PADDINGS));
    }

    @Test
    public void testNotInDesktopWindow_hideHeader() {
        setupDesktopWindowing(/* isInDesktopWindow= */ false, new Rect());
        WebAppHeaderLayoutMediator.setMinHeightForTesting(SYS_APP_HEADER_HEIGHT);

        final Rect initialPaddings = new Rect(0, 0, 0, 0);
        mModel.set(WebAppHeaderLayoutProperties.PADDINGS, initialPaddings);

        mMediator.onAppHeaderStateChanged(mAppHeaderState);
        assertEquals(
                "Header min height should match default min height",
                SYS_APP_HEADER_HEIGHT,
                mModel.get(WebAppHeaderLayoutProperties.MIN_HEIGHT));
        assertEquals(
                "Header paddings should match initial view paddings",
                initialPaddings,
                mModel.get(WebAppHeaderLayoutProperties.PADDINGS));
        assertFalse(
                "Header view should be gone", mModel.get(WebAppHeaderLayoutProperties.IS_VISIBLE));
    }

    @Test
    public void testHeaderInitiallyHidden_WidthSupplierUpdatesOnVisibilityChange() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);

        mMediator =
                new WebAppHeaderLayoutMediator(
                        mModel,
                        mHeaderDelegate,
                        mDesktopWindowStateManager,
                        mScrimManager,
                        mTabSupplier,
                        mHeaderControlPositionSupplier,
                        mThemeColorProvider,
                        SYS_APP_HEADER_HEIGHT,
                        HEADER_BUTTON_HEIGHT,
                        DisplayMode.MINIMAL_UI,
                        mSetHeaderAsOverlayCallback,
                        "Package name");
        mShadowLooper.idle();

        mModel.get(WebAppHeaderLayoutProperties.WIDTH_CHANGED_CALLBACK).onResult(SCREEN_WIDTH);

        // View starts off visible.
        assertTrue(
                "IS_VISIBLE property should be true.",
                mModel.get(WebAppHeaderLayoutProperties.IS_VISIBLE));
        assertEquals(
                "Width supplier should report SCREEN_WIDTH.",
                Integer.valueOf(SCREEN_WIDTH),
                mMediator.getWidthSupplierForTesting().get());

        // Change the app header state to have a View.GONE app header view.
        AppHeaderState goneState =
                new AppHeaderState(
                        WIDEST_UNOCCLUDED_RECT,
                        WIDEST_UNOCCLUDED_RECT,
                        /* isInDesktopWindow= */ false);
        mMediator.onAppHeaderStateChanged(goneState);
        mModel.get(WebAppHeaderLayoutProperties.VISIBILITY_CHANGED_CALLBACK).onResult(View.GONE);
        assertFalse(
                "IS_VISIBLE property should be false.",
                mModel.get(WebAppHeaderLayoutProperties.IS_VISIBLE));
        assertEquals(
                "Width supplier should be zero.",
                Integer.valueOf(0),
                mMediator.getWidthSupplierForTesting().get());
    }

    @Test
    public void testAppHeaderHeightIsLessThanMin_noTopPaddingsSet() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);

        mMediator.onAppHeaderStateChanged(mAppHeaderState);
        assertEquals(
                "Header min height should match default min height",
                SYS_APP_HEADER_HEIGHT,
                mModel.get(WebAppHeaderLayoutProperties.MIN_HEIGHT));
        assertEquals(
                "Vertical paddings should be 0 when system bar is less than min height",
                new Rect(LEFT_INSET, 0, RIGHT_INSET, 0),
                mModel.get(WebAppHeaderLayoutProperties.PADDINGS));
    }

    @Test
    public void testAppHeaderHeightIsGreaterThanMin_setTopPaddingEqualStatusBarHeight() {
        final int statusBarHeight = 10;
        final int headerHeight = SYS_APP_HEADER_HEIGHT + statusBarHeight;
        final Rect widestUnoccludedRect =
                new Rect(LEFT_INSET, statusBarHeight, SCREEN_WIDTH - RIGHT_INSET, headerHeight);
        setupDesktopWindowing(/* isInDesktopWindow= */ true, widestUnoccludedRect);
        WebAppHeaderLayoutMediator.setMinHeightForTesting(SYS_APP_HEADER_HEIGHT);

        mMediator.onAppHeaderStateChanged(mAppHeaderState);
        assertEquals(
                "Header min height should match app header height",
                headerHeight,
                mModel.get(WebAppHeaderLayoutProperties.MIN_HEIGHT));
        assertEquals(
                "Top padding should match exceeding size of the app header",
                new Rect(LEFT_INSET, 10, RIGHT_INSET, 0),
                mModel.get(WebAppHeaderLayoutProperties.PADDINGS));
    }

    @Test
    public void testGoBackWithHistory_shouldGoBack() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.WebAppHeader.BackButtonEvent", BackEvent.BACK);
        mTabSupplier.set(mTab);
        when(mTab.canGoBack()).thenReturn(true);

        mMediator.goBack();
        verify(mTab).goBack();
        watcher.assertExpected("Back event should be recorded.");
    }

    @Test
    public void testGoBackNoHistory_shouldNotGoBack() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.WebAppHeader.BackButtonEvent", BackEvent.INVALID);
        mTabSupplier.set(mTab);
        when(mTab.canGoBack()).thenReturn(false);

        mMediator.goBack();
        verify(mTab, never()).goBack();
        watcher.assertExpected("Invalid event should be recorded.");
    }

    @Test
    public void testGoBackNoTab_shouldNotGoBack() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.WebAppHeader.BackButtonEvent", BackEvent.INVALID);
        mMediator.goBack();
        verify(mTab, never()).goBack();
        watcher.assertExpected("Invalid event should be recorded.");
    }

    @Test
    public void testReload_shouldReloadTab() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.WebAppHeader.ReloadButtonEvent", ReloadType.RELOAD_FROM_CACHE);
        when(mTab.isLoading()).thenReturn(false);
        mTabSupplier.set(mTab);

        mMediator.refreshTab(false);
        verify(mTab).reload();
        watcher.assertExpected("Reload from cache should be recorded.");
    }

    @Test
    public void testReloadWhileReloading_shouldStopReloading() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.WebAppHeader.ReloadButtonEvent", ReloadType.STOP_RELOAD);
        when(mTab.isLoading()).thenReturn(true);
        mTabSupplier.set(mTab);

        mMediator.refreshTab(false);
        verify(mTab).stopLoading();
        watcher.assertExpected("Stop reloading should be recorded.");
    }

    @Test
    public void testReloadTabIgnoringCache_shouldReloadIgnoringCache() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.WebAppHeader.ReloadButtonEvent",
                        ReloadType.RELOAD_IGNORE_CACHE);
        when(mTab.isLoading()).thenReturn(false);
        mTabSupplier.set(mTab);

        mMediator.refreshTab(true);
        verify(mTab).reloadIgnoringCache();
        watcher.assertExpected("Reload ignoring cache should be recorded.");
    }

    @Test
    public void testNotInDW_AllAreaIsDraggable() {
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(null);
        mModel.get(WebAppHeaderLayoutProperties.WIDTH_CHANGED_CALLBACK).onResult(SCREEN_WIDTH);

        final var areas = mModel.get(WebAppHeaderLayoutProperties.NON_DRAGGABLE_AREAS);
        assertEquals("There should be only one area in the list", 1, areas.size());
        assertEquals(
                "The area should be an empty area that allows to drag everywhere",
                new Rect(0, 0, 0, 0),
                areas.get(0));
    }

    @Test
    public void testInDWButNotInWindow_AllAreaIsDraggable() {
        setupDesktopWindowing(/* isInDesktopWindow= */ false, WIDEST_UNOCCLUDED_RECT);

        final var nonDraggableAreas = List.of(new Rect(0, 0, 10, 10), new Rect(10, 0, 10, 10));
        mHeaderControlPositionSupplier.set(nonDraggableAreas);
        mMediator.onAppHeaderStateChanged(mAppHeaderState);

        mModel.get(WebAppHeaderLayoutProperties.WIDTH_CHANGED_CALLBACK).onResult(SCREEN_WIDTH);

        final var areas = mModel.get(WebAppHeaderLayoutProperties.NON_DRAGGABLE_AREAS);
        assertEquals("There should be only one area in the list", 1, areas.size());
        assertEquals(
                "The area should be an empty area that allows to drag everywhere",
                new Rect(0, 0, 0, 0),
                areas.get(0));
    }

    @Test
    public void testInDWInWindowWithLaidOutView_SetNonDraggableAreas() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);

        final var nonDraggableAreas = List.of(new Rect(0, 0, 10, 10), new Rect(10, 0, 10, 10));
        mHeaderControlPositionSupplier.set(nonDraggableAreas);

        mMediator.onAppHeaderStateChanged(mAppHeaderState);
        mModel.get(WebAppHeaderLayoutProperties.WIDTH_CHANGED_CALLBACK).onResult(SCREEN_WIDTH);
        mShadowLooper.idle();

        final var areas = mModel.get(WebAppHeaderLayoutProperties.NON_DRAGGABLE_AREAS);
        assertEquals("There should be only 2 non draggable areas", 2, areas.size());
        assertArrayEquals(
                "Non draggable areas from supplier should match model areas",
                areas.toArray(),
                mModel.get(WebAppHeaderLayoutProperties.NON_DRAGGABLE_AREAS).toArray());
    }

    @Test
    public void testInDwLayoutStructureChanges_SetNonDraggableAreaOnEachUpdate() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);

        // Setup layout without children.
        final List<Rect> initialNonDraggableArea = List.of();
        mHeaderControlPositionSupplier.set(initialNonDraggableArea);
        mMediator.onAppHeaderStateChanged(mAppHeaderState);
        mModel.get(WebAppHeaderLayoutProperties.WIDTH_CHANGED_CALLBACK).onResult(SCREEN_WIDTH);

        // Verify area is empty.
        var areas = mModel.get(WebAppHeaderLayoutProperties.NON_DRAGGABLE_AREAS);
        assertEquals("There should be only one area in the list", 1, areas.size());
        assertEquals(
                "The area should be an empty area that allows to drag everywhere",
                new Rect(0, 0, 0, 0),
                areas.get(0));

        // Children has laid out and layout update is sent with the same width.
        final var nonDraggableAreas = List.of(new Rect(0, 0, 10, 10), new Rect(10, 0, 10, 10));
        mHeaderControlPositionSupplier.set(nonDraggableAreas);
        mModel.get(WebAppHeaderLayoutProperties.WIDTH_CHANGED_CALLBACK).onResult(SCREEN_WIDTH);

        // Verify non-draggable area is updated.
        areas = mModel.get(WebAppHeaderLayoutProperties.NON_DRAGGABLE_AREAS);
        assertEquals("There should be only 2 non draggable areas", 2, areas.size());
        assertArrayEquals(
                "Non draggable areas from supplier should match model areas",
                areas.toArray(),
                mModel.get(WebAppHeaderLayoutProperties.NON_DRAGGABLE_AREAS).toArray());
    }

    @Test
    public void testSetInitialTheme() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);
        assertEquals(
                "Light color should be set initially",
                LIGHT_COLOR,
                mModel.get(WebAppHeaderLayoutProperties.BACKGROUND_COLOR));
        verify(mDesktopWindowStateManager).updateForegroundColor(LIGHT_COLOR);
    }

    @Test
    public void testThemeChanges_SetNewTheme() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);
        when(mThemeColorProvider.getThemeColor()).thenReturn(DARK_COLOR);

        mMediator.onThemeColorChanged(DARK_COLOR, /* shouldAnimate= */ false);
        assertEquals(
                "Dark color should be set initially",
                DARK_COLOR,
                mModel.get(WebAppHeaderLayoutProperties.BACKGROUND_COLOR));

        verify(mDesktopWindowStateManager).updateForegroundColor(DARK_COLOR);
    }

    @Test
    public void testScrimOverlaysWebContent_DisableHeaderControls() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);
        mMediator.getScrimVisibilityObserver().onResult(true);
        verify(mHeaderDelegate).disableControlsAndClearOldToken(TokenHolder.INVALID_TOKEN);
    }

    @Test
    public void testClearPreviousTokenAndAcquireNewOnSecondScrimOverlay() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);
        when(mHeaderDelegate.disableControlsAndClearOldToken(TokenHolder.INVALID_TOKEN))
                .thenReturn(0);
        mMediator.getScrimVisibilityObserver().onResult(true);
        verify(mHeaderDelegate).disableControlsAndClearOldToken(TokenHolder.INVALID_TOKEN);

        when(mHeaderDelegate.disableControlsAndClearOldToken(TokenHolder.INVALID_TOKEN))
                .thenReturn(1);
        mMediator.getScrimVisibilityObserver().onResult(true);
        verify(mHeaderDelegate).disableControlsAndClearOldToken(0);
    }

    @Test
    public void testScrimOverlaysAndThenHides_EnableHeaderControls() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);
        when(mHeaderDelegate.disableControlsAndClearOldToken(TokenHolder.INVALID_TOKEN))
                .thenReturn(0);

        mMediator.getScrimVisibilityObserver().onResult(true);
        verify(mHeaderDelegate).disableControlsAndClearOldToken(TokenHolder.INVALID_TOKEN);

        mMediator.getScrimVisibilityObserver().onResult(false);
        verify(mHeaderDelegate).releaseDisabledControlsToken(0);
    }

    @Test
    public void testWindowControlsOverlay_BrowserControlsVisible() {
        mMediator =
                new WebAppHeaderLayoutMediator(
                        mModel,
                        mHeaderDelegate,
                        mDesktopWindowStateManager,
                        mScrimManager,
                        mTabSupplier,
                        mHeaderControlPositionSupplier,
                        mThemeColorProvider,
                        SYS_APP_HEADER_HEIGHT,
                        HEADER_BUTTON_HEIGHT,
                        DisplayMode.WINDOW_CONTROLS_OVERLAY,
                        mSetHeaderAsOverlayCallback,
                        "Package Name");
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);
        mMediator.onAppHeaderStateChanged(mAppHeaderState);
        mMediator.setUserToggleHeaderAsOverlay(true);
        mMediator.setBrowserControlsVisible(true);
        verify(mSetHeaderAsOverlayCallback, times(2)).onResult(false);
        verify(mWebContents, times(3)).updateWindowControlsOverlay(new Rect());
        assertEquals(
                "Bars should be hidden when browser controls are visible",
                null,
                mModel.get(WebAppHeaderLayoutProperties.BACKGROUND_CUTOUTS));
    }

    @Test
    public void testWindowControlsOverlay_BrowserControlsInvisible() {
        mMediator =
                new WebAppHeaderLayoutMediator(
                        mModel,
                        mHeaderDelegate,
                        mDesktopWindowStateManager,
                        mScrimManager,
                        mTabSupplier,
                        mHeaderControlPositionSupplier,
                        mThemeColorProvider,
                        SYS_APP_HEADER_HEIGHT,
                        HEADER_BUTTON_HEIGHT,
                        DisplayMode.WINDOW_CONTROLS_OVERLAY,
                        mSetHeaderAsOverlayCallback,
                        "Package Name");
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);
        mMediator.onAppHeaderStateChanged(mAppHeaderState);
        mMediator.setUserToggleHeaderAsOverlay(true);
        mMediator.setBrowserControlsVisible(false);
        verify(mSetHeaderAsOverlayCallback, times(2)).onResult(true);
        verify(mWebContents, times(2)).updateWindowControlsOverlay(WIDEST_UNOCCLUDED_RECT);

        List<Rect> cutouts = mModel.get(WebAppHeaderLayoutProperties.BACKGROUND_CUTOUTS);
        assertEquals("There should be only one cutout", 1, cutouts.size());
        assertEquals(
                new Rect(LEFT_INSET, 0, SCREEN_WIDTH - RIGHT_INSET, SYS_APP_HEADER_HEIGHT),
                cutouts.get(0));
    }

    @Test
    public void testBackgroundBars_NoHeaderState() {
        verify(mWebContents).updateWindowControlsOverlay(new Rect());
        assertEquals(
                "Default value should be null",
                null,
                mModel.get(WebAppHeaderLayoutProperties.BACKGROUND_CUTOUTS));
    }

    @Test
    public void testBackgroundBars_HeaderAsOverlayFalse() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);
        mMediator.onAppHeaderStateChanged(mAppHeaderState);
        verify(mWebContents, times(2)).updateWindowControlsOverlay(new Rect());
        assertEquals(
                "Value should be null when not an overlay",
                null,
                mModel.get(WebAppHeaderLayoutProperties.BACKGROUND_CUTOUTS));
    }

    @Test
    public void testWindowControlsOverlay_NonDraggableArea() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);
        mMediator =
                new WebAppHeaderLayoutMediator(
                        mModel,
                        mHeaderDelegate,
                        mDesktopWindowStateManager,
                        mScrimManager,
                        mTabSupplier,
                        mHeaderControlPositionSupplier,
                        mThemeColorProvider,
                        SYS_APP_HEADER_HEIGHT,
                        HEADER_BUTTON_HEIGHT,
                        DisplayMode.WINDOW_CONTROLS_OVERLAY,
                        mSetHeaderAsOverlayCallback,
                        "Package Name");
        mMediator.onAppHeaderStateChanged(mAppHeaderState);
        mMediator.setUserToggleHeaderAsOverlay(true);
        mMediator.setBrowserControlsVisible(false);

        final var areasBefore = mModel.get(WebAppHeaderLayoutProperties.NON_DRAGGABLE_AREAS);
        assertEquals("There should be only one area in the list", 1, areasBefore.size());
        assertEquals(
                "Until onSystemGestureExclusionRectsChanged is called, the area is empty",
                EMPTY_NON_DRAGGABLE_AREA,
                areasBefore.get(0));

        mMediator.onSystemGestureExclusionRectsChanged(List.of(WIDEST_UNOCCLUDED_RECT));
        final var areasAfter = mModel.get(WebAppHeaderLayoutProperties.NON_DRAGGABLE_AREAS);
        assertEquals("There should be only one area in the list", 1, areasAfter.size());
        assertEquals(
                "The area should cover the whole unoccluded area",
                WIDEST_UNOCCLUDED_RECT,
                areasAfter.get(0));
    }

    @Test
    public void testWindowControlsOverlay_CombinedNonDraggableArea() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true, WIDEST_UNOCCLUDED_RECT);

        final var nonDraggableAreas = List.of(new Rect(0, 0, 10, 10), new Rect(10, 0, 10, 10));
        mHeaderControlPositionSupplier.set(nonDraggableAreas);

        mMediator =
                new WebAppHeaderLayoutMediator(
                        mModel,
                        mHeaderDelegate,
                        mDesktopWindowStateManager,
                        mScrimManager,
                        mTabSupplier,
                        mHeaderControlPositionSupplier,
                        mThemeColorProvider,
                        SYS_APP_HEADER_HEIGHT,
                        HEADER_BUTTON_HEIGHT,
                        DisplayMode.WINDOW_CONTROLS_OVERLAY,
                        mSetHeaderAsOverlayCallback,
                        "Package Name");
        mMediator.onAppHeaderStateChanged(mAppHeaderState);
        mMediator.setUserToggleHeaderAsOverlay(true);
        mMediator.setBrowserControlsVisible(false);
        mMediator.onSystemGestureExclusionRectsChanged(List.of(WIDEST_UNOCCLUDED_RECT));

        final var areas = mModel.get(WebAppHeaderLayoutProperties.NON_DRAGGABLE_AREAS);
        assertEquals("There should be only one area in the list", 3, areas.size());
        assertEquals(
                "The area should cover the whole unoccluded area",
                WIDEST_UNOCCLUDED_RECT,
                areas.get(2));
    }
}
