// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.ui.web_app_header.WebAppHeaderLayoutCoordinator.MIN_HEADER_WIDTH_DP;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Build;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup;
import org.chromium.chrome.browser.web_app_header.R;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.util.TokenHolder;

import java.util.HashSet;
import java.util.List;
import java.util.Locale;

@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
@EnableFeatures({ChromeFeatureList.ANDROID_MINIMAL_UI_LARGE_SCREEN})
public class WebAppHeaderLayoutCoordinatorTest {
    private static final int SCREEN_WIDTH = 800;
    private static final int SCREEN_HEIGHT = 1600;
    private static final int SYS_APP_HEADER_HEIGHT = 40;
    private static final int LEFT_INSET = 50;
    private static final int RIGHT_INSET = 60;
    private static final Rect WINDOW_RECT = new Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    private static final Rect WIDEST_UNOCCLUDED_RECT =
            new Rect(LEFT_INSET, 0, SCREEN_WIDTH - RIGHT_INSET, SYS_APP_HEADER_HEIGHT);

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock public DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock public Profile mProfile;
    @Mock public ThemeColorProvider mThemeColorProvider;
    @Mock public BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock public ScrimManager mScrimManager;
    @Mock public NavigationPopup.HistoryDelegate mHistoryDelegate;
    @Mock public WebappExtras mWebAppExtras;
    @Mock public Tab mTab;

    private WebAppHeaderLayoutCoordinator mCoordinator;
    private Activity mActivity;
    private ViewGroup mContentView;
    private ViewStub mViewStub;
    private ObservableSupplierImpl<Tab> mTabSupplier;
    private ObservableSupplierImpl<Boolean> mScrimVisibilitySupplier;
    private AppHeaderState mAppHeaderState;
    private ShadowLooper mShadowLooper;

    @Before
    public void setup() {
        mShadowLooper = shadowOf(Looper.getMainLooper());

        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.WEB_APK);
        setupDisplayMode(DisplayMode.STANDALONE);

        mScrimVisibilitySupplier = new ObservableSupplierImpl<>();
        when(mScrimManager.getScrimVisibilitySupplier()).thenReturn(mScrimVisibilitySupplier);

        mTabSupplier = new ObservableSupplierImpl<>();
        mActivityScenarioRule.getScenario().onActivity(testActivity -> mActivity = testActivity);
        mContentView = new FrameLayout(mActivity);
        mViewStub = new ViewStub(mActivity);
        mViewStub.setLayoutResource(R.layout.web_app_header_layout);
        mContentView.addView(mViewStub);
        mActivity.setContentView(mContentView);
    }

    private void createCoordinator() {
        mCoordinator =
                new WebAppHeaderLayoutCoordinator(
                        mViewStub,
                        mDesktopWindowStateManager,
                        mTabSupplier,
                        mThemeColorProvider,
                        mIntentDataProvider,
                        mScrimManager,
                        mHistoryDelegate);
    }

    private void setupDesktopWindowing(boolean isInDesktopWindow) {
        setupDesktopWindowing(WINDOW_RECT, WIDEST_UNOCCLUDED_RECT, isInDesktopWindow);
    }

    private void setupDesktopWindowing(
            Rect windowRect, Rect widestUnoccludedRect, boolean isInDesktopWindow) {
        mAppHeaderState = new AppHeaderState(windowRect, widestUnoccludedRect, isInDesktopWindow);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(mAppHeaderState);
    }

    private void notifyHeaderStateChanged() {
        var headerObserverCaptor =
                ArgumentCaptor.forClass(DesktopWindowStateManager.AppHeaderObserver.class);
        verify(mDesktopWindowStateManager, atLeastOnce())
                .addObserver(headerObserverCaptor.capture());

        for (var observer : headerObserverCaptor.getAllValues()) {
            // Notifying all observers is closer to the truth than relying on registration order.
            observer.onAppHeaderStateChanged(mAppHeaderState);
        }
    }

    private void setupDisplayMode(@DisplayMode.EnumType int displayMode) {
        mWebAppExtras =
                new WebappExtras(
                        "",
                        "",
                        "",
                        new WebappIcon(),
                        "",
                        "",
                        displayMode,
                        0,
                        0,
                        0,
                        0,
                        0,
                        false,
                        false,
                        false);

        when(mIntentDataProvider.getWebappExtras()).thenReturn(mWebAppExtras);
        when(mIntentDataProvider.getResolvedDisplayMode()).thenReturn(displayMode);
    }

    private void setupTab(boolean isLoading, boolean canGoBack) {
        when(mTab.isLoading()).thenReturn(isLoading);
        when(mTab.canGoBack()).thenReturn(canGoBack);
        mTabSupplier.set(mTab);
    }

    private void verifyControlsEnabledState(boolean isEnabled) {
        assertEquals(
                String.format(
                        Locale.US,
                        "Reload button should be %s",
                        isEnabled ? "enabled" : "disabled"),
                isEnabled,
                mActivity.findViewById(R.id.refresh_button).isEnabled());
        assertEquals(
                String.format(
                        Locale.US, "Back button should be %s", isEnabled ? "enabled" : "disabled"),
                isEnabled,
                mActivity.findViewById(R.id.back_button).isEnabled());
    }

    private void verifyControlsVisibility(int expectedVisibility) {
        assertEquals(
                String.format(
                        Locale.US,
                        "Reload button visibility should be %s",
                        expectedVisibility == View.VISIBLE ? "visible" : "gone"),
                expectedVisibility,
                mActivity.findViewById(R.id.refresh_button).getVisibility());
        assertEquals(
                String.format(
                        Locale.US,
                        "Back button visibility should be %s",
                        expectedVisibility == View.VISIBLE ? "visible" : "gone"),
                expectedVisibility,
                mActivity.findViewById(R.id.back_button).getVisibility());
    }

    private void verifyHeaderContainsNonDraggableAreas(List<Rect> expectedNonDraggableAreas) {
        var expectedNonDraggableSet = new HashSet<>(expectedNonDraggableAreas);
        var headerView = mContentView.findViewById(R.id.web_app_header_layout);
        var nonDraggableAreas = headerView.getSystemGestureExclusionRects();
        assertEquals(
                "Header non-draggable areas size should match expected areas size",
                expectedNonDraggableAreas.size(),
                nonDraggableAreas.size());

        for (var rect : nonDraggableAreas) {
            assertTrue(
                    String.format(
                            Locale.US,
                            "Header should not contain non-draggable area=%s",
                            rect.toString()),
                    expectedNonDraggableSet.contains(rect));
        }
    }

    private void verifyWholeHeaderIsDraggable() {
        // Empty rect is expected, because Android SDK keeps previous list of rects if null or empty
        // list is passed.
        verifyHeaderContainsNonDraggableAreas(List.of(new Rect(0, 0, 0, 0)));
    }

    private void testDisplayModeUMA(@DisplayMode.EnumType int displayMode) {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("CustomTabs.WebAppHeader.DisplayMode2", displayMode)
                        .build();

        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(displayMode);
        setupTab(/* isLoading= */ false, /* canGoBack= */ true);
        createCoordinator();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testInitNoAppHeaderState_shouldNotInitCoordinator() {
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(null);
        createCoordinator();

        assertNull(
                "Web app header should not be inflated when not in a desktop window",
                mActivity.findViewById(R.id.web_app_header_layout));
    }

    @Test
    public void testInitHasAppHeaderState_shouldInitCoordinatorImmediately() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        createCoordinator();

        assertNotNull(
                "Web app header should be inflated when in a desktop window",
                mActivity.findViewById(R.id.web_app_header_layout));
    }

    @Test
    public void testMinUiDisplayMode_shouldMakeMinUiVisible() {
        // Init header in a window with enough space and wait for flexible area and layout updates
        // to propagate.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();
        mShadowLooper.idle();

        // Verify min ui controls are in consistent state and non-draggable area is updated.
        var reloadButton = mActivity.findViewById(R.id.refresh_button);
        var backButton = mActivity.findViewById(R.id.back_button);

        verifyControlsVisibility(View.VISIBLE);
        assertTrue("Reload button should be enabled", reloadButton.isEnabled());
        assertFalse("Back button should be disabled", backButton.isEnabled());
        verifyHeaderContainsNonDraggableAreas(mCoordinator.collectNonDraggableAreas());
    }

    @Test
    public void testMinUiMinimizeWindow_ControlsDoNotFit_HideControls() {
        // Init header in a window with enough space and wait for flexible area and layout updates
        // to propagate.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();
        mShadowLooper.idle();

        // Emulate minimizing window.
        int flexibleAreaWidth = MIN_HEADER_WIDTH_DP - 1;
        setupDesktopWindowing(
                new Rect(0, 0, LEFT_INSET + flexibleAreaWidth + RIGHT_INSET, SCREEN_HEIGHT),
                new Rect(LEFT_INSET, 0, LEFT_INSET + flexibleAreaWidth, SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        // Verify buttons are not visible and the whole header is draggable.
        verifyControlsVisibility(View.GONE);
        verifyWholeHeaderIsDraggable();
    }

    @Test
    public void testMinUiMaximizeWindow_ControlsFit_ShowControls() {
        // Emulate minimized window.
        int flexibleAreaWidth = MIN_HEADER_WIDTH_DP - 1;
        setupDesktopWindowing(
                new Rect(0, 0, LEFT_INSET + flexibleAreaWidth + RIGHT_INSET, SCREEN_HEIGHT),
                new Rect(LEFT_INSET, 0, LEFT_INSET + flexibleAreaWidth, SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);

        setupDisplayMode(DisplayMode.MINIMAL_UI);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();
        mShadowLooper.idle();

        // Emulate maximizing window.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        // Verify buttons visible and draggable area is updated.
        verifyControlsVisibility(View.VISIBLE);
        verifyHeaderContainsNonDraggableAreas(mCoordinator.collectNonDraggableAreas());
    }

    @Test
    public void testMinUiMinimizeWindow_MinimumWidthMatchThreshold_KeepControlsVisible() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();

        setupDesktopWindowing(
                new Rect(0, 0, LEFT_INSET + MIN_HEADER_WIDTH_DP + RIGHT_INSET, SCREEN_HEIGHT),
                new Rect(LEFT_INSET, 0, LEFT_INSET + MIN_HEADER_WIDTH_DP, SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);

        notifyHeaderStateChanged();
        verifyControlsVisibility(View.VISIBLE);
    }

    @Test
    public void testDisableControls() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI);
        setupTab(/* isLoading= */ false, /* canGoBack= */ true);
        createCoordinator();

        mShadowLooper.idle();

        mCoordinator.disableControlsAndClearOldToken(TokenHolder.INVALID_TOKEN);
        verifyControlsEnabledState(false);
    }

    @Test
    public void testEnableControls() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI);
        setupTab(/* isLoading= */ false, /* canGoBack= */ true);
        createCoordinator();

        mShadowLooper.idle();

        int token = mCoordinator.disableControlsAndClearOldToken(TokenHolder.INVALID_TOKEN);
        verifyControlsEnabledState(false);

        mCoordinator.releaseDisabledControlsToken(token);
        verifyControlsEnabledState(true);
    }

    @Test
    public void testDisableControlsOnManyTokens() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI);
        setupTab(/* isLoading= */ false, /* canGoBack= */ true);
        createCoordinator();

        mShadowLooper.idle();

        int firstToken = mCoordinator.disableControlsAndClearOldToken(TokenHolder.INVALID_TOKEN);
        mCoordinator.disableControlsAndClearOldToken(TokenHolder.INVALID_TOKEN);
        verifyControlsEnabledState(false);

        mCoordinator.releaseDisabledControlsToken(firstToken);
        verifyControlsEnabledState(false);
    }

    @Test
    public void testControlsVisibilityChangeUMA() {
        // Emulate minimizing window.
        int flexibleAreaWidth = MIN_HEADER_WIDTH_DP - 1;
        setupDesktopWindowing(
                new Rect(0, 0, LEFT_INSET + flexibleAreaWidth + RIGHT_INSET, SCREEN_HEIGHT),
                new Rect(LEFT_INSET, 0, LEFT_INSET + flexibleAreaWidth, SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI);
        setupTab(/* isLoading= */ false, /* canGoBack= */ true);
        createCoordinator();

        mShadowLooper.idle();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("CustomTabs.WebAppHeader.ControlsShownTime2")
                        .expectAnyRecord("CustomTabs.WebAppHeader.ControlsHiddenTime2")
                        .build();

        // Emulate maximizing window.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        // Emulate minimizing window.
        setupDesktopWindowing(
                new Rect(0, 0, LEFT_INSET + flexibleAreaWidth + RIGHT_INSET, SCREEN_HEIGHT),
                new Rect(LEFT_INSET, 0, LEFT_INSET + flexibleAreaWidth, SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        // Emulate maximizing window.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testDisplayModeBrowserUMA() {
        testDisplayModeUMA(DisplayMode.BROWSER);
    }

    @Test
    public void testDisplayModeMinimalUIUMA() {
        testDisplayModeUMA(DisplayMode.MINIMAL_UI);
    }

    @Test
    public void testDisplayModeStandaloneUMA() {
        testDisplayModeUMA(DisplayMode.STANDALONE);
    }

    @Test
    public void testDisplayModeFullscreenUMA() {
        testDisplayModeUMA(DisplayMode.FULLSCREEN);
    }

    @Test
    public void testDisplayModeWindowControlsOverlayUMA() {
        testDisplayModeUMA(DisplayMode.WINDOW_CONTROLS_OVERLAY);
    }

    @Test
    public void testDisplayModeTabbedUMA() {
        testDisplayModeUMA(DisplayMode.TABBED);
    }

    @Test
    public void testDisplayModeBorderlessUMA() {
        testDisplayModeUMA(DisplayMode.BORDERLESS);
    }

    @Test
    public void testDisplayModePiPUMA() {
        testDisplayModeUMA(DisplayMode.PICTURE_IN_PICTURE);
    }
}
