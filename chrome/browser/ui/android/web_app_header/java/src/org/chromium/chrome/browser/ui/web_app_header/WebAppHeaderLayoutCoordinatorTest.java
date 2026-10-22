// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build;
import android.os.Looper;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;
import android.widget.TextView;

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

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionsToolbarCoordinator;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.web_app_header.R;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.webapps.WebappsUtils;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.util.TokenHolder;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;

@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
public class WebAppHeaderLayoutCoordinatorTest {
    private static final int SCREEN_WIDTH = 800;
    private static final int SCREEN_HEIGHT = 1600;
    private static final int SYS_APP_HEADER_HEIGHT = 40;
    private static final int LEFT_INSET = 50;
    private static final int RIGHT_INSET = 60;
    private static final String CLIENT_PACKAGE_NAME = "Package name";
    private static final String TEST_APP_PACKAGE = "com.example.twa";
    private static final String TEST_APP_LABEL = "Example App";
    // Longer than web_app_header_app_name_max_width in px. Robolectric's default Paint measures
    // 1px per character, so this name is wider than both the min and max width thresholds.
    private static final String LONG_APP_LABEL = "A".repeat(200);
    private static final Rect WINDOW_RECT = new Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    private static final Rect WIDEST_UNOCCLUDED_RECT =
            new Rect(LEFT_INSET, 0, SCREEN_WIDTH - RIGHT_INSET, SYS_APP_HEADER_HEIGHT);
    private static final int HEADER_CONTROL_BUTTON_DP = 48;
    private static final int BUTTON_PADDING_DP = 4;
    // With the default fixture the caption bar (SYS_APP_HEADER_HEIGHT) is shorter than the header
    // buttons, so the mediator asks controls to inset their content by the difference to stay
    // vertically aligned with the system caption icons. Robolectric runs at density 1.0, so
    // dp == px.
    private static final int EXPECTED_BUTTON_BOTTOM_INSET_PX =
            HEADER_CONTROL_BUTTON_DP - SYS_APP_HEADER_HEIGHT;

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
    @Mock public WebContents mWebContents;
    @Mock public Callback<Boolean> mSetHeaderAsOverlayCallback;
    @Mock public BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private Runnable mRequestRenderRunnable;
    @Mock private ActivityWindowAndroid mWindowAndroid;
    @Mock public TabModelSelector mTabModelSelector;
    @Mock public TabCreator mTabCreator;
    @Mock public ModalDialogManager mModalDialogManager;

    private WebAppHeaderLayoutCoordinator mCoordinator;
    private Activity mActivity;
    private ViewGroup mContentView;
    private ViewStub mViewStub;
    private SettableNullableObservableSupplier<Tab> mTabSupplier;
    private SettableNonNullObservableSupplier<Boolean> mScrimVisibilitySupplier;
    private AppHeaderState mAppHeaderState;
    private ShadowLooper mShadowLooper;
    private OneshotSupplierImpl<AppMenuCoordinator> mAppMenuSupplier;
    private OneshotSupplierImpl<ChromeAndroidTask> mChromeAndroidTaskSupplier;

    @Before
    public void setup() {
        mShadowLooper = shadowOf(Looper.getMainLooper());
        mChromeAndroidTaskSupplier = new OneshotSupplierImpl<>();

        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.WEB_APK);
        setupDisplayMode(DisplayMode.STANDALONE);

        mScrimVisibilitySupplier = ObservableSuppliers.createNonNull(false);
        when(mScrimManager.getScrimVisibilitySupplier()).thenReturn(mScrimVisibilitySupplier);

        mTabSupplier = ObservableSuppliers.createNullable();
        mActivityScenarioRule.getScenario().onActivity(testActivity -> mActivity = testActivity);
        doReturn(mWindowAndroid).when(mTab).getWindowAndroid();
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mActivity));
        mContentView = new FrameLayout(mActivity);
        mViewStub = new ViewStub(mActivity);
        mViewStub.setLayoutResource(R.layout.web_app_header_layout);
        mContentView.addView(mViewStub);
        mActivity.setContentView(mContentView);

        mAppMenuSupplier = new OneshotSupplierImpl<>();
    }

    private void createCoordinator() {
        createCoordinator(CLIENT_PACKAGE_NAME);
    }

    private void createCoordinator(String clientPackageName) {
        mCoordinator =
                new WebAppHeaderLayoutCoordinator(
                        mActivity,
                        mViewStub,
                        mDesktopWindowStateManager,
                        mTabSupplier,
                        mThemeColorProvider,
                        mIntentDataProvider,
                        mScrimManager,
                        mHistoryDelegate,
                        mSetHeaderAsOverlayCallback,
                        mBrowserControlsStateProvider,
                        mAppMenuSupplier,
                        null,
                        mWindowAndroid,
                        mRequestRenderRunnable,
                        clientPackageName,
                        mChromeAndroidTaskSupplier,
                        mTabModelSelector,
                        mTabCreator,
                        mModalDialogManager);
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
        setupDisplayMode(displayMode, /* shortName= */ "");
    }

    /**
     * Stubs the intent data provider as a WebAPK with the given display mode and short name. A
     * non-null {@link WebappExtras} is what makes the coordinator treat the app as a WebAPK.
     */
    private void setupDisplayMode(@DisplayMode.EnumType int displayMode, String shortName) {
        mWebAppExtras =
                new WebappExtras(
                        "",
                        "",
                        "",
                        new WebappIcon(),
                        "",
                        shortName,
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

    /**
     * Stubs the intent data provider as a TWA. Unlike WebAPKs, TWAs carry no {@link WebappExtras},
     * which is what makes the coordinator resolve the app name from the client package instead.
     */
    private void setupTwa() {
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        when(mIntentDataProvider.getWebappExtras()).thenReturn(null);
    }

    /**
     * Sets up a WebAPK in WINDOW_CONTROLS_OVERLAY mode with every precondition the mediator needs
     * to actually enter overlay mode: desktop window, WCO display mode, browser controls hidden,
     * and a tab with {@link WebContents}. Without the last two the mediator either vetoes the
     * overlay or flips its state without notifying the coordinator.
     */
    private void setupWcoWithAppName() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.WINDOW_CONTROLS_OVERLAY, TEST_APP_LABEL);
        when(mBrowserControlsStateProvider.getAndroidControlsVisibility())
                .thenReturn(View.INVISIBLE);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
    }

    /** The toggle reacts to ACTION_UP via an OnTouchListener, so performClick() won't fire it. */
    private void tapWcoToggle() {
        View toggle = mActivity.findViewById(R.id.wco_toggle_button);
        assertEquals("Toggle must be visible to be tapped", View.VISIBLE, toggle.getVisibility());
        MotionEvent up = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0);
        toggle.dispatchTouchEvent(up);
        up.recycle();
        mShadowLooper.idle();
    }

    /**
     * Registers a fake installed app with Robolectric's PackageManager.
     *
     * @param packageName The package to register.
     * @param label The app's android:label, or null to simulate an app without a label.
     */
    private void installApp(String packageName, String label) {
        ApplicationInfo appInfo = new ApplicationInfo();
        appInfo.packageName = packageName;
        appInfo.nonLocalizedLabel = label;
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.applicationInfo = appInfo;
        shadowOf(mActivity.getPackageManager()).installPackage(packageInfo);
    }

    private TextView getAppNameView() {
        return mActivity.findViewById(R.id.app_name);
    }

    private int getDimenPx(int id) {
        return mActivity.getResources().getDimensionPixelSize(id);
    }

    /**
     * Emulates the right-aligned controls being laid out at {@code widthPx}, e.g. after the origin
     * text or extensions toolbar appears. Mirrors how the extensions tests drive geometry with
     * {@link View#layout}. Explicit layout params keep the width stable across any relayout.
     */
    private void layoutRightControls(int widthPx) {
        View wrapper = mActivity.findViewById(R.id.right_aligned_wrapper);
        ViewGroup.LayoutParams lp = wrapper.getLayoutParams();
        lp.width = widthPx;
        wrapper.setLayoutParams(lp);
        wrapper.layout(0, 0, widthPx, SYS_APP_HEADER_HEIGHT);
        // The layout listener defers the re-evaluation with View#post.
        mShadowLooper.idle();
    }

    private void setupTab(boolean isLoading, boolean canGoBack) {
        when(mTab.isLoading()).thenReturn(isLoading);
        when(mTab.canGoBack()).thenReturn(canGoBack);
        mTabSupplier.set(mTab);
    }

    // Helper to determine the total width for buttons used based on display mode.
    private int getMinButtonWidth(@DisplayMode.EnumType int displayMode) {
        int totalWidth = 0;

        if (displayMode == DisplayMode.MINIMAL_UI) {
            // Back and reload buttons.
            totalWidth += HEADER_CONTROL_BUTTON_DP * 2;
        } else if (displayMode == DisplayMode.WINDOW_CONTROLS_OVERLAY) {
            // Toggle button, plus a 2-button buffer for the header content.
            totalWidth += HEADER_CONTROL_BUTTON_DP * 3;
        }
        return totalWidth + BUTTON_PADDING_DP;
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

    private void verifyControlsVisibility(
            @DisplayMode.EnumType int displayMode, int expectedVisibility) {
        if (displayMode == DisplayMode.MINIMAL_UI) {
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
        } else if (displayMode == DisplayMode.WINDOW_CONTROLS_OVERLAY) {
            assertEquals(
                    String.format(
                            Locale.US,
                            "Toggle button visibility should be %s",
                            expectedVisibility == View.VISIBLE ? "visible" : "gone"),
                    expectedVisibility,
                    mActivity.findViewById(R.id.wco_toggle_button).getVisibility());
        } else if (displayMode == DisplayMode.STANDALONE) {
            assertEquals(
                    String.format(
                            Locale.US,
                            "Menu button visibility should be %s",
                            expectedVisibility == View.VISIBLE ? "visible" : "gone"),
                    expectedVisibility,
                    expectedVisibility == View.VISIBLE
                            ? mActivity.findViewById(R.id.menu_button).getVisibility()
                            : mActivity.findViewById(R.id.menu_button_wrapper).getVisibility());
        }
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

        View headerView = mActivity.findViewById(R.id.web_app_header_layout);
        assertNotNull("Web app header should be inflated when in a desktop window", headerView);
        assertEquals(
                "Header layout height should match system caption bar height",
                SYS_APP_HEADER_HEIGHT,
                headerView.getLayoutParams().height);
    }

    @Test
    public void testLazyInit_NoAppHeaderStateAtCreation_ControlsDoNotFit_HideControls() {
        // Start with no app header state so the coordinator defers initialization.
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(null);
        setupDisplayMode(DisplayMode.MINIMAL_UI);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();
        mShadowLooper.idle();
        assertNull(
                "Web app header should not be inflated before entering a desktop window",
                mActivity.findViewById(R.id.web_app_header_layout));

        // Enter desktop windowing with a header too narrow for the min-UI controls.
        int flexibleAreaWidth = getMinButtonWidth(DisplayMode.MINIMAL_UI) - 1;
        setupDesktopWindowing(
                new Rect(0, 0, LEFT_INSET + flexibleAreaWidth + RIGHT_INSET, SCREEN_HEIGHT),
                new Rect(LEFT_INSET, 0, LEFT_INSET + flexibleAreaWidth, SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        // Verify header was lazily inflated, buttons are hidden and whole header is draggable.
        assertNotNull(
                "Web app header should be inflated when in a desktop window",
                mActivity.findViewById(R.id.web_app_header_layout));
        verifyControlsVisibility(DisplayMode.MINIMAL_UI, View.GONE);
        verifyWholeHeaderIsDraggable();
    }

    @Test
    public void testLazyInit_NoAppHeaderStateAtCreation_ControlsFit_ShowControls() {
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(null);
        setupDisplayMode(DisplayMode.MINIMAL_UI);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();
        mShadowLooper.idle();

        // Enter desktop windowing with plenty of space.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        // Verify buttons visible and draggable area is updated.
        verifyControlsVisibility(DisplayMode.MINIMAL_UI, View.VISIBLE);
        verifyHeaderContainsNonDraggableAreas(mCoordinator.collectControlPositions());
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

        verifyControlsVisibility(DisplayMode.MINIMAL_UI, View.VISIBLE);
        assertTrue("Reload button should be enabled", reloadButton.isEnabled());
        assertFalse("Back button should be disabled", backButton.isEnabled());
        verifyHeaderContainsNonDraggableAreas(mCoordinator.collectControlPositions());
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
        int flexibleAreaWidth = getMinButtonWidth(DisplayMode.MINIMAL_UI) - 1;
        setupDesktopWindowing(
                new Rect(0, 0, LEFT_INSET + flexibleAreaWidth + RIGHT_INSET, SCREEN_HEIGHT),
                new Rect(LEFT_INSET, 0, LEFT_INSET + flexibleAreaWidth, SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        // Verify buttons are not visible and the whole header is draggable.
        verifyControlsVisibility(DisplayMode.MINIMAL_UI, View.GONE);
        verifyWholeHeaderIsDraggable();
    }

    @Test
    public void testMinUiMaximizeWindow_ControlsFit_ShowControls() {
        // Emulate minimized window.
        int flexibleAreaWidth = getMinButtonWidth(DisplayMode.MINIMAL_UI) - 1;
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
        verifyControlsVisibility(DisplayMode.MINIMAL_UI, View.VISIBLE);
        verifyHeaderContainsNonDraggableAreas(mCoordinator.collectControlPositions());
    }

    @Test
    public void testMinUiMinimizeWindow_MinimumWidthMatchThreshold_KeepControlsVisible() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();

        setupDesktopWindowing(
                new Rect(
                        0,
                        0,
                        LEFT_INSET + getMinButtonWidth(DisplayMode.MINIMAL_UI) + RIGHT_INSET,
                        SCREEN_HEIGHT),
                new Rect(
                        LEFT_INSET,
                        0,
                        LEFT_INSET + getMinButtonWidth(DisplayMode.MINIMAL_UI),
                        SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);

        notifyHeaderStateChanged();
        verifyControlsVisibility(DisplayMode.MINIMAL_UI, View.VISIBLE);
    }

    @Test
    public void testWCOMinimizeWindow_ControlsDoNotFit_HideControls() {
        // Init header in a window with enough space and wait for flexible area and layout updates
        // to propagate.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.WINDOW_CONTROLS_OVERLAY);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();
        mShadowLooper.idle();

        // Emulate minimizing window.
        int flexibleAreaWidth = getMinButtonWidth(DisplayMode.WINDOW_CONTROLS_OVERLAY) - 1;
        setupDesktopWindowing(
                new Rect(0, 0, LEFT_INSET + flexibleAreaWidth + RIGHT_INSET, SCREEN_HEIGHT),
                new Rect(LEFT_INSET, 0, LEFT_INSET + flexibleAreaWidth, SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        // Verify buttons are not visible and the whole header is draggable.
        verifyControlsVisibility(DisplayMode.WINDOW_CONTROLS_OVERLAY, View.GONE);
        verifyWholeHeaderIsDraggable();
    }

    @Test
    public void testWCOMaximizeWindow_ControlsFit_ShowControls() {
        // Emulate minimized window.
        int flexibleAreaWidth = getMinButtonWidth(DisplayMode.WINDOW_CONTROLS_OVERLAY) - 1;
        setupDesktopWindowing(
                new Rect(0, 0, LEFT_INSET + flexibleAreaWidth + RIGHT_INSET, SCREEN_HEIGHT),
                new Rect(LEFT_INSET, 0, LEFT_INSET + flexibleAreaWidth, SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);

        setupDisplayMode(DisplayMode.WINDOW_CONTROLS_OVERLAY);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();
        mShadowLooper.idle();

        // Emulate maximizing window.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        // Verify buttons visible and draggable area is updated.
        verifyControlsVisibility(DisplayMode.WINDOW_CONTROLS_OVERLAY, View.VISIBLE);
        verifyHeaderContainsNonDraggableAreas(mCoordinator.collectControlPositions());
    }

    @Test
    public void testWCOMinimizeWindow_MinimumWidthMatchThreshold_KeepControlsVisible() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.WINDOW_CONTROLS_OVERLAY);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();

        setupDesktopWindowing(
                new Rect(
                        0,
                        0,
                        LEFT_INSET
                                + getMinButtonWidth(DisplayMode.WINDOW_CONTROLS_OVERLAY)
                                + RIGHT_INSET,
                        SCREEN_HEIGHT),
                new Rect(
                        LEFT_INSET,
                        0,
                        LEFT_INSET + getMinButtonWidth(DisplayMode.WINDOW_CONTROLS_OVERLAY),
                        SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);

        notifyHeaderStateChanged();
        verifyControlsVisibility(DisplayMode.WINDOW_CONTROLS_OVERLAY, View.VISIBLE);
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
    public void testMinUiMinimizeWindow_ControlsDoNotFit_HideControls_MenuButtonVisible() {
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();
        mShadowLooper.idle();

        // Emulate minimizing window with added Menu button.
        int flexibleAreaWidth =
                getMinButtonWidth(DisplayMode.MINIMAL_UI) + HEADER_CONTROL_BUTTON_DP - 1;
        setupDesktopWindowing(
                new Rect(0, 0, LEFT_INSET + flexibleAreaWidth + RIGHT_INSET, SCREEN_HEIGHT),
                new Rect(LEFT_INSET, 0, LEFT_INSET + flexibleAreaWidth, SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        verifyControlsVisibility(DisplayMode.MINIMAL_UI, View.GONE);
        var menuButton = mActivity.findViewById(R.id.menu_button_wrapper);
        assertTrue("Menu button should be gone", menuButton.getVisibility() == View.GONE);
        verifyWholeHeaderIsDraggable();
    }

    @Test
    public void testMinUiMaximizeWindow_ControlsFit_ShowControls_MenuButtonVisible() {
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        // Emulate minimized window with added Menu button.
        int flexibleAreaWidth =
                getMinButtonWidth(DisplayMode.MINIMAL_UI) + HEADER_CONTROL_BUTTON_DP - 1;
        setupDesktopWindowing(
                new Rect(0, 0, LEFT_INSET + flexibleAreaWidth + RIGHT_INSET, SCREEN_HEIGHT),
                new Rect(LEFT_INSET, 0, LEFT_INSET + flexibleAreaWidth, SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);

        setupDisplayMode(DisplayMode.MINIMAL_UI);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();
        mShadowLooper.idle();

        // Maximize window.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        // Buttons should be visible and undraggable.
        verifyControlsVisibility(DisplayMode.MINIMAL_UI, View.VISIBLE);
        var menuButton = mActivity.findViewById(R.id.menu_button);
        assertTrue("Menu button should be visible", menuButton.getVisibility() == View.VISIBLE);
        verifyHeaderContainsNonDraggableAreas(mCoordinator.collectControlPositions());
    }

    @Test
    public void testMinUiWindow_ShowControls_MenuButtonVisible() {
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();
        mShadowLooper.idle();

        var reloadButton = mActivity.findViewById(R.id.refresh_button);
        var backButton = mActivity.findViewById(R.id.back_button);
        var menuButton = mActivity.findViewById(R.id.menu_button);

        verifyControlsVisibility(DisplayMode.MINIMAL_UI, View.VISIBLE);
        assertTrue("Menu button should be visible", menuButton.getVisibility() == View.VISIBLE);

        assertTrue("Reload button should be enabled", reloadButton.isEnabled());
        assertFalse("Back button should be enabled", backButton.isEnabled());
        assertTrue("Menu button should be enabled", menuButton.isEnabled());
    }

    @Test
    public void testStandaloneMaximizeWindow_ControlsFit_ShowControls_MenuButtonVisible() {
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        // Emulate minimized window with added Menu button.
        int flexibleAreaWidth =
                getMinButtonWidth(DisplayMode.STANDALONE) + HEADER_CONTROL_BUTTON_DP - 1;
        setupDesktopWindowing(
                new Rect(0, 0, LEFT_INSET + flexibleAreaWidth + RIGHT_INSET, SCREEN_HEIGHT),
                new Rect(LEFT_INSET, 0, LEFT_INSET + flexibleAreaWidth, SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);

        setupDisplayMode(DisplayMode.STANDALONE);
        createCoordinator();
        mShadowLooper.idle();

        // Maximize window.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        // Buttons should be visible and undraggable.
        verifyControlsVisibility(DisplayMode.STANDALONE, View.VISIBLE);
        var menuButton = mActivity.findViewById(R.id.menu_button);
        assertTrue("Menu button should be visible", menuButton.getVisibility() == View.VISIBLE);
        verifyHeaderContainsNonDraggableAreas(mCoordinator.collectControlPositions());
    }

    @Test
    public void testStandaloneWindow_ShowControls_MenuButtonVisible() {
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.STANDALONE);
        createCoordinator();
        mShadowLooper.idle();

        var menuButton = mActivity.findViewById(R.id.menu_button);

        verifyControlsVisibility(DisplayMode.STANDALONE, View.VISIBLE);
        assertTrue("Menu button should be visible", menuButton.getVisibility() == View.VISIBLE);
        assertTrue("Menu button should be enabled", menuButton.isEnabled());
    }

    @Test
    public void testStandaloneWindow_DuplicateMenuButtonWrapper_BindsToCorrectView() {
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        // Add a MenuButton with ID R.id.menu_button_wrapper to the Activity before the
        // coordinator is created to simulate the CustomTabToolbar view containing the same ID.
        LayoutInflater.from(mActivity).inflate(R.layout.menu_button, mContentView, true);

        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.STANDALONE);
        createCoordinator();
        mShadowLooper.idle();

        // Emulate minimizing window so controls do not fit.
        int flexibleAreaWidth =
                getMinButtonWidth(DisplayMode.STANDALONE) + HEADER_CONTROL_BUTTON_DP - 1;
        setupDesktopWindowing(
                new Rect(0, 0, LEFT_INSET + flexibleAreaWidth + RIGHT_INSET, SCREEN_HEIGHT),
                new Rect(LEFT_INSET, 0, LEFT_INSET + flexibleAreaWidth, SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        var headerLayout = mCoordinator.getWebAppHeaderLayout();
        assertNotNull("WebAppHeaderLayout should be initialized", headerLayout);
        var headerMenuContainer = headerLayout.findViewById(R.id.web_app_menu_button_wrapper);
        var headerMenuButton = headerLayout.findViewById(R.id.menu_button);
        assertNotNull("Menu button should exist in header", headerMenuButton);

        // In minimized window, header controls do not fit and whole header is draggable.
        assertEquals(
                "Header menu button container should be gone when controls do not fit",
                View.GONE,
                headerMenuContainer.getVisibility());
        verifyWholeHeaderIsDraggable();

        // Maximize window so controls fit.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        // Menu button in header should become visible, enabled, and excluded from drag.
        assertEquals(
                "Header menu button container should be visible when controls fit",
                View.VISIBLE,
                headerMenuContainer.getVisibility());
        assertEquals(
                "Header menu button should be visible",
                View.VISIBLE,
                headerMenuButton.getVisibility());
        assertTrue("Header menu button should be enabled", headerMenuButton.isEnabled());
        verifyHeaderContainsNonDraggableAreas(mCoordinator.collectControlPositions());
    }

    @Test
    public void testControlsVisibilityChangeUMA() {
        // Emulate minimizing window.
        int flexibleAreaWidth = getMinButtonWidth(DisplayMode.MINIMAL_UI) - 1;
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
    public void testDisplayModeUnframedUMA() {
        testDisplayModeUMA(DisplayMode.UNFRAMED);
    }

    @Test
    public void testDisplayModePiPUMA() {
        testDisplayModeUMA(DisplayMode.PICTURE_IN_PICTURE);
    }

    @Test
    public void testOriginTextViewShowsCorrectDomain() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI);
        WebappsUtils.setIsTwaInstallerPackageForTesting(true);

        GURL testUrl = new GURL("https://www.example.com/path/to/page");
        when(mIntentDataProvider.getUrlToLoad()).thenReturn(testUrl.getSpec());
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        when(mIntentDataProvider.getAllTrustedWebActivityOrigins())
                .thenReturn(Collections.singleton(Origin.create(testUrl.getOrigin().getSpec())));
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);

        createCoordinator();
        mShadowLooper.idle();

        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        when(navigationHandle.getUrl()).thenReturn(testUrl);
        // Simulate finished navigation.
        mCoordinator.onDidFinishNavigationInPrimaryMainFrame(mTab, navigationHandle);

        TextView originTextView = mActivity.findViewById(R.id.origin);
        assertNotNull("Origin TextView should not be null", originTextView);
        assertEquals(
                "Origin TextView should show the correct domain",
                UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(testUrl),
                originTextView.getText().toString());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_TWA_DISCLOSURES)
    public void
            testOriginTextViewShowsCorrectDomain_withDisclosuresFlagEnabled_noTwaInstaller_onTablet() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI);
        // Explicitly set TWA Installer to false
        WebappsUtils.setIsTwaInstallerPackageForTesting(false);

        GURL testUrl = new GURL("https://www.example.com/path/to/page");
        when(mIntentDataProvider.getUrlToLoad()).thenReturn(testUrl.getSpec());
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        when(mIntentDataProvider.getAllTrustedWebActivityOrigins())
                .thenReturn(Collections.singleton(Origin.create(testUrl.getOrigin().getSpec())));
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);

        createCoordinator();
        mShadowLooper.idle();

        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        when(navigationHandle.getUrl()).thenReturn(testUrl);
        // Simulate finished navigation.
        mCoordinator.onDidFinishNavigationInPrimaryMainFrame(mTab, navigationHandle);

        TextView originTextView = mActivity.findViewById(R.id.origin);
        assertNotNull("Origin TextView should not be null", originTextView);
        assertEquals(
                "Origin TextView should show the correct domain",
                UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(testUrl),
                originTextView.getText().toString());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_TWA_DISCLOSURES)
    public void testOriginTextViewIsGone_withDisclosuresFlagEnabled_noTwaInstaller_onNonTablet() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI);
        // Explicitly set TWA Installer to false
        WebappsUtils.setIsTwaInstallerPackageForTesting(false);

        GURL testUrl = new GURL("https://www.example.com/path/to/page");
        when(mIntentDataProvider.getUrlToLoad()).thenReturn(testUrl.getSpec());
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        when(mIntentDataProvider.getAllTrustedWebActivityOrigins())
                .thenReturn(Collections.singleton(Origin.create(testUrl.getOrigin().getSpec())));
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);

        createCoordinator();
        mShadowLooper.idle();

        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        when(navigationHandle.getUrl()).thenReturn(testUrl);
        // Simulate finished navigation.
        mCoordinator.onDidFinishNavigationInPrimaryMainFrame(mTab, navigationHandle);

        TextView originTextView = mActivity.findViewById(R.id.origin);
        assertNotNull("Origin TextView should not be null", originTextView);
        assertEquals(View.GONE, originTextView.getVisibility());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @DisableFeatures(ChromeFeatureList.DESKTOP_ANDROID_TWA_DISCLOSURES)
    public void testOriginTextViewIsGone_withDisclosuresFlagDisabled_noTwaInstaller() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI);
        // Explicitly set TWA Installer to false
        WebappsUtils.setIsTwaInstallerPackageForTesting(false);

        GURL testUrl = new GURL("https://www.example.com/path/to/page");
        when(mIntentDataProvider.getUrlToLoad()).thenReturn(testUrl.getSpec());
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        when(mIntentDataProvider.getAllTrustedWebActivityOrigins())
                .thenReturn(Collections.singleton(Origin.create(testUrl.getOrigin().getSpec())));
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);

        createCoordinator();
        mShadowLooper.idle();

        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        when(navigationHandle.getUrl()).thenReturn(testUrl);
        // Simulate finished navigation.
        mCoordinator.onDidFinishNavigationInPrimaryMainFrame(mTab, navigationHandle);

        TextView originTextView = mActivity.findViewById(R.id.origin);
        assertNotNull("Origin TextView should not be null", originTextView);
        assertEquals(View.GONE, originTextView.getVisibility());
    }

    @Test
    public void testWindowControlsOverlayToggleButtonColor() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.WINDOW_CONTROLS_OVERLAY);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();

        var tint = mock(ColorStateList.class);
        mCoordinator.onTintChanged(tint, tint, BrandedColorScheme.APP_DEFAULT);
        assertEquals(
                "Tint change should be propagated to the toggle button",
                mCoordinator.getToggleButtonImageTintList(),
                tint);
    }

    @Test
    public void testTintObserverRegisteredOnCreationAndRemovedOnDestroy() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.STANDALONE);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();

        verify(mThemeColorProvider).addTintObserver(mCoordinator);

        mCoordinator.destroy();
        verify(mThemeColorProvider).removeTintObserver(mCoordinator);
    }

    @Test
    public void testTabObserverClearedOnTabChangeAndDestroy() {
        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);
        doReturn(mWindowAndroid).when(tab1).getWindowAndroid();
        doReturn(mWindowAndroid).when(tab2).getWindowAndroid();

        // 1. Initial Tab Setup
        mTabSupplier.set(tab1);
        createCoordinator();
        mShadowLooper.idle();

        // Verify observer added to tab1
        verify(tab1).addObserver(mCoordinator);

        // 2. Tab Change
        mTabSupplier.set(tab2);
        mShadowLooper.idle();

        // Verify observer removed from tab1 and added to tab2
        verify(tab1).removeObserver(mCoordinator);
        verify(tab2).addObserver(mCoordinator);

        // 3. Coordinator Destroy
        mCoordinator.destroy();
        mShadowLooper.idle();

        // Verify observer removed from tab2
        verify(tab2).removeObserver(mCoordinator);
    }

    @Test
    public void testExtensionsToolbarCoordinator_InitializedWhenTaskAvailable() {
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.STANDALONE);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);

        TabModel tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.getCurrentModel()).thenReturn(tabModel);

        createCoordinator();
        mShadowLooper.idle();

        // Before ChromeAndroidTask is available, extensions toolbar is not yet initialized.
        assertNull(mCoordinator.getExtensionsToolbarCoordinator());

        // Provide ChromeAndroidTask.
        ChromeAndroidTask task = mock(ChromeAndroidTask.class);
        ExtensionsToolbarCoordinator mockToolbarCoordinator =
                mock(ExtensionsToolbarCoordinator.class);
        when(task.addFeature(any(), any())).thenReturn(mockToolbarCoordinator);
        mChromeAndroidTaskSupplier.set(task);
        mShadowLooper.idle();

        // Verify ExtensionsToolbarCoordinator is initialized.
        assertNotNull(mCoordinator.getExtensionsToolbarCoordinator());
        assertEquals(mockToolbarCoordinator, mCoordinator.getExtensionsToolbarCoordinator());
    }

    @Test
    public void testExtensionsToolbarCoordinator_NotInitializedForWebApk() {
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.WEB_APK);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.STANDALONE);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);

        TabModel tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.getCurrentModel()).thenReturn(tabModel);

        createCoordinator();
        mShadowLooper.idle();

        ChromeAndroidTask task = mock(ChromeAndroidTask.class);
        mChromeAndroidTaskSupplier.set(task);
        mShadowLooper.idle();

        assertNull(mCoordinator.getExtensionsToolbarCoordinator());
        verify(task, never()).addFeature(any(), any());
    }

    @Test
    public void testExtensionsToolbarCoordinator_NotInitializedWhenHeaderNotInflated() {
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(null);
        setupDisplayMode(DisplayMode.STANDALONE);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);

        TabModel tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.getCurrentModel()).thenReturn(tabModel);

        createCoordinator();
        mShadowLooper.idle();

        ChromeAndroidTask task = mock(ChromeAndroidTask.class);
        mChromeAndroidTaskSupplier.set(task);
        mShadowLooper.idle();

        assertNull(mCoordinator.getExtensionsToolbarCoordinator());
        verify(task, never()).addFeature(any(), any());
    }

    @Test
    public void testExtensionsToolbarCoordinator_NotInitializedWhenProfileNull() {
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.STANDALONE);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);

        TabModel tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(null);
        when(mTabModelSelector.getCurrentModel()).thenReturn(tabModel);

        createCoordinator();
        mShadowLooper.idle();

        ChromeAndroidTask task = mock(ChromeAndroidTask.class);
        mChromeAndroidTaskSupplier.set(task);
        mShadowLooper.idle();

        assertNull(mCoordinator.getExtensionsToolbarCoordinator());
        verify(task, never()).addFeature(any(), any());
    }

    private void testExtensionsToolbarCoordinator_InitializedInDisplayMode(
            @DisplayMode.EnumType int displayMode) {
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(displayMode);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);

        TabModel tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.getCurrentModel()).thenReturn(tabModel);

        createCoordinator();
        mShadowLooper.idle();

        ChromeAndroidTask task = mock(ChromeAndroidTask.class);
        ExtensionsToolbarCoordinator mockToolbarCoordinator =
                mock(ExtensionsToolbarCoordinator.class);
        when(task.addFeature(any(), any())).thenReturn(mockToolbarCoordinator);
        mChromeAndroidTaskSupplier.set(task);
        mShadowLooper.idle();

        assertNotNull(
                "ExtensionsToolbarCoordinator should be initialized for display mode "
                        + displayMode,
                mCoordinator.getExtensionsToolbarCoordinator());
        assertEquals(mockToolbarCoordinator, mCoordinator.getExtensionsToolbarCoordinator());
    }

    private void testExtensionsToolbarCoordinator_NotInitializedInDisplayMode(
            @DisplayMode.EnumType int displayMode) {
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(displayMode);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);

        TabModel tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.getCurrentModel()).thenReturn(tabModel);

        createCoordinator();
        mShadowLooper.idle();

        ChromeAndroidTask task = mock(ChromeAndroidTask.class);
        mChromeAndroidTaskSupplier.set(task);
        mShadowLooper.idle();

        assertNull(
                "ExtensionsToolbarCoordinator should not be initialized for display mode "
                        + displayMode,
                mCoordinator.getExtensionsToolbarCoordinator());
        verify(task, never()).addFeature(any(), any());
    }

    @Test
    public void testExtensionsToolbarCoordinator_InitializedInMinimalUi() {
        testExtensionsToolbarCoordinator_InitializedInDisplayMode(DisplayMode.MINIMAL_UI);
    }

    @Test
    public void testExtensionsToolbarCoordinator_InitializedInStandalone() {
        testExtensionsToolbarCoordinator_InitializedInDisplayMode(DisplayMode.STANDALONE);
    }

    @Test
    public void testExtensionsToolbarCoordinator_InitializedInWindowControlsOverlay() {
        testExtensionsToolbarCoordinator_InitializedInDisplayMode(
                DisplayMode.WINDOW_CONTROLS_OVERLAY);
    }

    @Test
    public void testExtensionsToolbarCoordinator_NotInitializedInBrowser() {
        testExtensionsToolbarCoordinator_NotInitializedInDisplayMode(DisplayMode.BROWSER);
    }

    @Test
    public void testExtensionsToolbarCoordinator_NotInitializedInFullscreen() {
        testExtensionsToolbarCoordinator_NotInitializedInDisplayMode(DisplayMode.FULLSCREEN);
    }

    @Test
    public void testExtensionsToolbarCoordinator_NotInitializedInTabbed() {
        testExtensionsToolbarCoordinator_NotInitializedInDisplayMode(DisplayMode.TABBED);
    }

    @Test
    public void testExtensionsToolbarCoordinator_NotInitializedInUnframed() {
        testExtensionsToolbarCoordinator_NotInitializedInDisplayMode(DisplayMode.UNFRAMED);
    }

    @Test
    public void testExtensionsToolbarCoordinator_NotInitializedInPictureInPicture() {
        testExtensionsToolbarCoordinator_NotInitializedInDisplayMode(
                DisplayMode.PICTURE_IN_PICTURE);
    }

    @Test
    public void testExtensionsToolbarCoordinator_DestroyedOnCoordinatorDestroy() {
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.STANDALONE);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);

        TabModel tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.getCurrentModel()).thenReturn(tabModel);

        createCoordinator();
        mShadowLooper.idle();

        ChromeAndroidTask task = mock(ChromeAndroidTask.class);
        ExtensionsToolbarCoordinator mockToolbarCoordinator =
                mock(ExtensionsToolbarCoordinator.class);
        when(task.addFeature(any(), any())).thenReturn(mockToolbarCoordinator);
        mChromeAndroidTaskSupplier.set(task);
        mShadowLooper.idle();

        assertEquals(mockToolbarCoordinator, mCoordinator.getExtensionsToolbarCoordinator());

        mCoordinator.destroy();
        verify(mockToolbarCoordinator).destroy();
        assertNull(mCoordinator.getExtensionsToolbarCoordinator());
    }

    @Test
    public void testExtensionsToolbar_NonDraggableAreaIncludedWhenVisible() {
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.STANDALONE);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);

        createCoordinator();
        mShadowLooper.idle();

        // Add a view representing the inflated extensions toolbar container.
        ViewGroup rightAlignedWrapper = mActivity.findViewById(R.id.right_aligned_wrapper);
        assertNotNull("Right aligned wrapper should be present", rightAlignedWrapper);
        View extensionsContainer = new View(mActivity);
        extensionsContainer.setId(R.id.extensions_toolbar_container);
        rightAlignedWrapper.addView(extensionsContainer);
        extensionsContainer.setVisibility(View.VISIBLE);
        extensionsContainer.layout(10, 0, 50, 40);

        notifyHeaderStateChanged();
        mShadowLooper.idle();

        List<Rect> nonDraggableAreas = mCoordinator.collectControlPositions();
        assertFalse("Non-draggable areas should not be empty", nonDraggableAreas.isEmpty());
        verifyHeaderContainsNonDraggableAreas(nonDraggableAreas);
    }

    @Test
    public void
            testMinimizeWindow_Standalone_MenuButtonAndExtensionsToolbar_DragExclusionUpdated() {
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.STANDALONE);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);

        createCoordinator();
        mShadowLooper.idle();

        // Add extensions toolbar container in default (GONE) state.
        ViewGroup rightAlignedWrapper = mActivity.findViewById(R.id.right_aligned_wrapper);
        assertNotNull("Right aligned wrapper should be present", rightAlignedWrapper);
        View extensionsContainer = new View(mActivity);
        extensionsContainer.setId(R.id.extensions_toolbar_container);
        extensionsContainer.setVisibility(View.GONE);
        rightAlignedWrapper.addView(extensionsContainer);

        // Emulate minimizing window where controls do not fit.
        int flexibleAreaWidth =
                getMinButtonWidth(DisplayMode.STANDALONE) + HEADER_CONTROL_BUTTON_DP - 1;
        setupDesktopWindowing(
                new Rect(0, 0, LEFT_INSET + flexibleAreaWidth + RIGHT_INSET, SCREEN_HEIGHT),
                new Rect(LEFT_INSET, 0, LEFT_INSET + flexibleAreaWidth, SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        // When minimized, controls do not fit and whole header is draggable.
        var headerLayout = mCoordinator.getWebAppHeaderLayout();
        assertNotNull("WebAppHeaderLayout should be initialized", headerLayout);
        var headerMenuContainer = headerLayout.findViewById(R.id.web_app_menu_button_wrapper);
        assertEquals(
                "Header menu button container should be gone when controls do not fit",
                View.GONE,
                headerMenuContainer.getVisibility());
        verifyWholeHeaderIsDraggable();

        // Maximize window so controls fit.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        // Menu button should be visible, enabled, and excluded from drag.
        assertEquals(
                "Header menu button container should be visible when controls fit",
                View.VISIBLE,
                headerMenuContainer.getVisibility());
        verifyHeaderContainsNonDraggableAreas(mCoordinator.collectControlPositions());

        // When extensions toolbar becomes visible, it is also included in non-draggable areas.
        extensionsContainer.setVisibility(View.VISIBLE);
        extensionsContainer.layout(10, 0, 50, 40);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        List<Rect> nonDraggableAreasWithExtensions = mCoordinator.collectControlPositions();
        assertTrue(
                "Non-draggable areas should include both menu button and extensions container",
                nonDraggableAreasWithExtensions.size() >= 2);
        verifyHeaderContainsNonDraggableAreas(nonDraggableAreasWithExtensions);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_TwaClientWithLabel_IsShown() {
        installApp(TEST_APP_PACKAGE, TEST_APP_LABEL);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupTwa();

        createCoordinator(TEST_APP_PACKAGE);
        // Run the background label lookup and deliver its result along with the initial
        // unoccluded width, both of which are posted to the main looper.
        RobolectricUtil.runAllBackgroundAndUi();

        TextView appNameView = getAppNameView();
        assertNotNull("App name view should be inflated", appNameView);
        assertEquals(
                "App name should be visible for a TWA client with a label",
                View.VISIBLE,
                appNameView.getVisibility());
        assertEquals(
                "App name should show the client app's label",
                TEST_APP_LABEL,
                appNameView.getText().toString());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_WebApk_ShowsShortNameFromIntentExtras() {
        // TEST_APP_PACKAGE is intentionally not installed: the WebAPK path must not consult
        // PackageManager and instead use the name already carried in the intent extras.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.STANDALONE, TEST_APP_LABEL);

        // WebAPKs have no Custom Tabs session, so no client package is passed in.
        createCoordinator(/* clientPackageName= */ null);
        // Deliver the initial unoccluded width, which is posted to the main looper.
        mShadowLooper.idle();

        TextView appNameView = getAppNameView();
        assertEquals(
                "App name should be visible for a WebAPK with a short name",
                View.VISIBLE,
                appNameView.getVisibility());
        assertEquals(
                "App name should show the WebAPK's short name",
                TEST_APP_LABEL,
                appNameView.getText().toString());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_WebApkWithoutShortName_IsHidden() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.STANDALONE, /* shortName= */ "");

        createCoordinator(/* clientPackageName= */ null);
        mShadowLooper.idle();

        TextView appNameView = getAppNameView();
        assertEquals(
                "App name should be gone when the WebAPK has no short name",
                View.GONE,
                appNameView.getVisibility());
        assertTrue(
                "No text should be set when the WebAPK has no short name",
                TextUtils.isEmpty(appNameView.getText()));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_PackageNotInstalled_IsHidden() {
        // TEST_APP_PACKAGE is intentionally not installed.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupTwa();

        createCoordinator(TEST_APP_PACKAGE);
        // Let the background label lookup fail and deliver its (null) result.
        RobolectricUtil.runAllBackgroundAndUi();

        TextView appNameView = getAppNameView();
        assertEquals(
                "App name should be gone, not just invisible, when the package is not found",
                View.GONE,
                appNameView.getVisibility());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_AppWithoutLabel_IsHidden() {
        // Without android:label, PackageManager falls back to the package name as the label.
        installApp(TEST_APP_PACKAGE, /* label= */ null);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupTwa();

        createCoordinator(TEST_APP_PACKAGE);
        // Run the background label lookup and deliver its result.
        RobolectricUtil.runAllBackgroundAndUi();

        TextView appNameView = getAppNameView();
        assertEquals(
                "App name should be gone when the app has no label",
                View.GONE,
                appNameView.getVisibility());
        assertTrue(
                "Raw package name should never be shown as the app name",
                TextUtils.isEmpty(appNameView.getText()));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_TintChange_UpdatesTextColor() {
        installApp(TEST_APP_PACKAGE, TEST_APP_LABEL);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupTwa();
        createCoordinator(TEST_APP_PACKAGE);
        // Run the background label lookup and deliver its result.
        RobolectricUtil.runAllBackgroundAndUi();

        ColorStateList tint = ColorStateList.valueOf(Color.RED);
        mCoordinator.onTintChanged(tint, tint, BrandedColorScheme.APP_DEFAULT);

        assertEquals(
                "Tint change should be propagated to the app name",
                tint,
                getAppNameView().getTextColors());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_TwaLabelArrivesAfterDestroy_IsIgnored() {
        installApp(TEST_APP_PACKAGE, TEST_APP_LABEL);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupTwa();
        createCoordinator(TEST_APP_PACKAGE);

        // Tear the coordinator down while the background label lookup is still pending, then let
        // the lookup complete.
        mCoordinator.destroy();
        RobolectricUtil.runAllBackgroundAndUi();

        TextView appNameView = getAppNameView();
        assertEquals(
                "A label arriving after destroy() must not be applied",
                View.GONE,
                appNameView.getVisibility());
        assertTrue(
                "A label arriving after destroy() must not be applied",
                TextUtils.isEmpty(appNameView.getText()));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_WcoToggledOnAndOff_HidesAndShowsName() {
        setupWcoWithAppName();
        createCoordinator(/* clientPackageName= */ null);
        mShadowLooper.idle();

        TextView appNameView = getAppNameView();
        assertEquals(
                "App name should be visible while the header is opaque",
                View.VISIBLE,
                appNameView.getVisibility());
        verifyControlsVisibility(DisplayMode.WINDOW_CONTROLS_OVERLAY, View.VISIBLE);

        // Overlay on: the web content draws under the header, so the name must yield.
        // Clear first: the constructor already reported onResult(false).
        clearInvocations((Object) mSetHeaderAsOverlayCallback);
        tapWcoToggle();
        verify(mSetHeaderAsOverlayCallback, atLeastOnce()).onResult(true);
        verify(mSetHeaderAsOverlayCallback, never()).onResult(false);
        assertEquals(
                "App name should be hidden when the header is drawn as an overlay",
                View.GONE,
                appNameView.getVisibility());
        // Controls still fit, so the name was hidden by the overlay, not by width.
        verifyControlsVisibility(DisplayMode.WINDOW_CONTROLS_OVERLAY, View.VISIBLE);

        // Overlay off again: the name comes back without needing a width change.
        clearInvocations((Object) mSetHeaderAsOverlayCallback);
        tapWcoToggle();
        verify(mSetHeaderAsOverlayCallback, atLeastOnce()).onResult(false);
        verify(mSetHeaderAsOverlayCallback, never()).onResult(true);
        assertEquals(
                "App name should be visible again after the overlay is turned off",
                View.VISIBLE,
                appNameView.getVisibility());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_WcoEnabledFromPrefsAtStartup_StartsHidden() {
        // The user turned on the overlay for this client package in a previous session.
        ChromeSharedPreferences.getInstance()
                .addToStringSet(
                        ChromePreferenceKeys.WINDOW_CONTROLS_OVERLAY_ENABLED_PACKAGES,
                        TEST_APP_PACKAGE);
        try {
            setupWcoWithAppName();
            createCoordinator(TEST_APP_PACKAGE);
            mShadowLooper.idle();

            TextView appNameView = getAppNameView();
            assertEquals(
                    "App name should start hidden when the overlay is already enabled",
                    View.GONE,
                    appNameView.getVisibility());
            assertEquals(
                    "App name text should still be bound so it can appear once overlay is off",
                    TEST_APP_LABEL,
                    appNameView.getText().toString());

            // Turning the overlay off at the same width shows the name. That proves the
            // start-hidden state came from the overlay, not from a lack of room.
            clearInvocations((Object) mSetHeaderAsOverlayCallback);
            tapWcoToggle();
            assertEquals(
                    "App name should appear when the user turns the overlay off",
                    View.VISIBLE,
                    appNameView.getVisibility());
        } finally {
            // Don't leak the persisted toggle into other tests.
            ChromeSharedPreferences.getInstance()
                    .removeFromStringSet(
                            ChromePreferenceKeys.WINDOW_CONTROLS_OVERLAY_ENABLED_PACKAGES,
                            TEST_APP_PACKAGE);
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_WcoOverlay_WinsOverAvailableWidth() {
        setupWcoWithAppName();
        createCoordinator(/* clientPackageName= */ null);
        mShadowLooper.idle();

        int roomyWidthPx =
                mCoordinator.calculateUIControlsMinWidth()
                        + mCoordinator.getAppNameWidthPxForTesting()
                        + 200;
        resizeFlexibleArea(roomyWidthPx);
        assertEquals(
                "Precondition: the name fits at this width",
                View.VISIBLE,
                getAppNameView().getVisibility());

        tapWcoToggle();
        assertEquals(View.GONE, getAppNameView().getVisibility());

        // A real width change while in overlay must not bring the name back.
        resizeFlexibleArea(roomyWidthPx + 100);
        assertEquals(
                "Width updates must not show the name while the overlay is on",
                View.GONE,
                getAppNameView().getVisibility());
        verifyControlsVisibility(DisplayMode.WINDOW_CONTROLS_OVERLAY, View.VISIBLE);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_MinimizeWindow_ControlsFitButNameDoesNot_HideNameKeepControls() {
        // Init header in a window with enough space for both the controls and the app name.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI, TEST_APP_LABEL);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator(/* clientPackageName= */ null);
        mShadowLooper.idle();
        int appNameWidth = mCoordinator.getAppNameWidthPxForTesting();
        assertTrue("App name should have a non-zero width", appNameWidth > 0);

        // Emulate minimizing the window so the controls still fit but the app name doesn't.
        resizeFlexibleArea(getMinButtonWidth(DisplayMode.MINIMAL_UI) + appNameWidth - 1);

        // The app name yields first; the controls stay usable.
        assertEquals(
                "App name should be hidden when there is no room for it",
                View.GONE,
                getAppNameView().getVisibility());
        verifyControlsVisibility(DisplayMode.MINIMAL_UI, View.VISIBLE);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_MaximizeWindow_NameFits_ShowName() {
        // Init header in a window where only the controls fit.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI, TEST_APP_LABEL);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator(/* clientPackageName= */ null);
        mShadowLooper.idle();
        int appNameWidth = mCoordinator.getAppNameWidthPxForTesting();
        resizeFlexibleArea(getMinButtonWidth(DisplayMode.MINIMAL_UI) + appNameWidth - 1);
        assertEquals(View.GONE, getAppNameView().getVisibility());

        // Emulate maximizing the window.
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        assertEquals(
                "App name should be shown again once there is room for it",
                View.VISIBLE,
                getAppNameView().getVisibility());
        verifyControlsVisibility(DisplayMode.MINIMAL_UI, View.VISIBLE);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_MinimizeWindow_WidthMatchesThreshold_KeepNameVisible() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.MINIMAL_UI, TEST_APP_LABEL);
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator(/* clientPackageName= */ null);
        mShadowLooper.idle();
        int appNameWidth = mCoordinator.getAppNameWidthPxForTesting();

        // Emulate a window with exactly enough room for the controls and the app name.
        resizeFlexibleArea(getMinButtonWidth(DisplayMode.MINIMAL_UI) + appNameWidth);

        assertEquals(
                "App name should stay visible when it exactly fits",
                View.VISIBLE,
                getAppNameView().getVisibility());
        verifyControlsVisibility(DisplayMode.MINIMAL_UI, View.VISIBLE);
    }

    /** Emulates resizing the window so that the header's unoccluded area is {@code width} px. */
    private void resizeFlexibleArea(int width) {
        setupDesktopWindowing(
                new Rect(0, 0, LEFT_INSET + width + RIGHT_INSET, SCREEN_HEIGHT),
                new Rect(LEFT_INSET, 0, LEFT_INSET + width, SYS_APP_HEADER_HEIGHT),
                /* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_FlagDisabled_IsNeverShown() {
        // TWA client is installed with a label and the window is wide. The only
        // thing standing between us and a visible name is the flag.
        installApp(TEST_APP_PACKAGE, TEST_APP_LABEL);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupTwa();

        createCoordinator(TEST_APP_PACKAGE);
        RobolectricUtil.runAllBackgroundAndUi();
        TextView appNameView = getAppNameView();
        assertEquals(
                "App name should be gone when the flag is disabled",
                View.GONE,
                appNameView.getVisibility());
        assertTrue(
                "App name text should never be set when the flag is disabled",
                TextUtils.isEmpty(appNameView.getText()));
        assertEquals(
                "No header width should be reserved for the app name when the flag is disabled",
                0,
                mCoordinator.getAppNameWidthPxForTesting());
    }

    @Test
    public void testGetClientAppLabel() {
        assertNull(
                "Uninstalled package should yield no label",
                WebAppHeaderUtils.getClientAppLabel(TEST_APP_PACKAGE));

        installApp(TEST_APP_PACKAGE, /* label= */ null);
        assertNull(
                "Package name fallback should be treated as no label",
                WebAppHeaderUtils.getClientAppLabel(TEST_APP_PACKAGE));

        installApp(TEST_APP_PACKAGE, TEST_APP_LABEL);
        assertEquals(
                TEST_APP_LABEL, WebAppHeaderUtils.getClientAppLabel(TEST_APP_PACKAGE).toString());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_WebApk_AppliesButtonBottomInset() {
        // WebAPK: the name is bound synchronously during init, before the mediator delivers the
        // initial inset, so the padding must come from onButtonBottomInsetChanged().
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.STANDALONE, TEST_APP_LABEL);
        createCoordinator(/* clientPackageName= */ null);
        mShadowLooper.idle();

        assertTrue("Fixture should produce a non-zero inset", EXPECTED_BUTTON_BOTTOM_INSET_PX > 0);
        assertEquals(
                "App name should be inset like the other header controls",
                EXPECTED_BUTTON_BOTTOM_INSET_PX,
                getAppNameView().getPaddingBottom());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_TwaLabelArrivesAfterInset_AppliesButtonBottomInset() {
        // TWA: the label resolves asynchronously, after the mediator has already delivered the
        // inset, so the padding must be applied when the view is bound in setAppName().
        installApp(TEST_APP_PACKAGE, TEST_APP_LABEL);
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupTwa();
        createCoordinator(TEST_APP_PACKAGE);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(
                "App name bound after the inset callback should still be inset",
                EXPECTED_BUTTON_BOTTOM_INSET_PX,
                getAppNameView().getPaddingBottom());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_InsetChange_UpdatesPadding() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.STANDALONE, TEST_APP_LABEL);
        createCoordinator(/* clientPackageName= */ null);
        mShadowLooper.idle();
        assertEquals(EXPECTED_BUTTON_BOTTOM_INSET_PX, getAppNameView().getPaddingBottom());

        // The caption bar grows to the button height: no inset is needed any more.
        setupDesktopWindowing(
                WINDOW_RECT,
                new Rect(LEFT_INSET, 0, SCREEN_WIDTH - RIGHT_INSET, HEADER_CONTROL_BUTTON_DP),
                /* isInDesktopWindow= */ true);
        notifyHeaderStateChanged();
        mShadowLooper.idle();

        assertEquals(
                "App name padding should follow inset changes",
                0,
                getAppNameView().getPaddingBottom());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_LongName_RequiredWidthCappedAtMinWidth() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.STANDALONE, LONG_APP_LABEL);
        createCoordinator(/* clientPackageName= */ null);
        mShadowLooper.idle();

        int minWidthPx = getDimenPx(R.dimen.web_app_header_app_name_min_width);
        int marginsPx = 2 * getDimenPx(R.dimen.header_button_padding);
        assertEquals(
                "A long name should only reserve the ellipsizable minimum plus its margins",
                minWidthPx + marginsPx,
                mCoordinator.getAppNameWidthPxForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_LongName_MaxWidthClampsToAvailableRoom() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.STANDALONE, LONG_APP_LABEL);
        createCoordinator(/* clientPackageName= */ null);
        mShadowLooper.idle();

        int minWidthPx = getDimenPx(R.dimen.web_app_header_app_name_min_width);
        int maxWidthPx = getDimenPx(R.dimen.web_app_header_app_name_max_width);
        int marginsPx = 2 * getDimenPx(R.dimen.header_button_padding);
        int controlsPx = getMinButtonWidth(DisplayMode.STANDALONE);
        TextView appNameView = getAppNameView();

        // Plenty of room: the name is bounded by its design maximum, not by the window.
        resizeFlexibleArea(controlsPx + marginsPx + maxWidthPx + 300);
        assertEquals(View.VISIBLE, appNameView.getVisibility());
        assertEquals("Wide window should cap at max width", maxWidthPx, appNameView.getMaxWidth());

        // Room between min and max: the name shrinks to exactly what is left for it.
        int midWidthPx = (minWidthPx + maxWidthPx) / 2;
        resizeFlexibleArea(controlsPx + marginsPx + midWidthPx);
        assertEquals(View.VISIBLE, appNameView.getVisibility());
        assertEquals(
                "Mid-size window should bound the name to the remaining room",
                midWidthPx,
                appNameView.getMaxWidth());

        // Exactly the minimum: still shown, at the ellipsizable floor.
        resizeFlexibleArea(controlsPx + marginsPx + minWidthPx);
        assertEquals(View.VISIBLE, appNameView.getVisibility());
        assertEquals(
                "Narrow window should floor at min width", minWidthPx, appNameView.getMaxWidth());

        // One px below the minimum: hidden rather than ellipsized further.
        resizeFlexibleArea(controlsPx + marginsPx + minWidthPx - 1);
        assertEquals(
                "Below min width the name should hide, not shrink",
                View.GONE,
                appNameView.getVisibility());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_WEB_APP_HEADER_APP_NAME)
    public void testAppName_RightControlsGrow_HidesAndRestoresName() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupDisplayMode(DisplayMode.STANDALONE, TEST_APP_LABEL);
        createCoordinator(/* clientPackageName= */ null);
        mShadowLooper.idle();

        // Size the window so there is room for the name plus a small slack beyond the static
        // controls estimate, which counts the origin text and extensions toolbar as 0.
        int appNameWidthPx = mCoordinator.getAppNameWidthPxForTesting();
        int slackPx = 20;
        resizeFlexibleArea(getMinButtonWidth(DisplayMode.STANDALONE) + appNameWidthPx + slackPx);
        TextView appNameView = getAppNameView();
        assertEquals(
                "Name should fit before the right controls expand",
                View.VISIBLE,
                appNameView.getVisibility());

        // The right-aligned controls grow past the slack (e.g. origin text appears). No
        // unoccluded-width change happens, so only the layout listener can catch this.
        int wrapperMarginPx = getDimenPx(R.dimen.header_button_padding);
        layoutRightControls(slackPx + wrapperMarginPx + 1);
        assertEquals(
                "Layout listener should hide the name once the controls eat its room",
                View.GONE,
                appNameView.getVisibility());

        // The controls shrink back (e.g. origin text fades out): the name returns.
        layoutRightControls(0);
        assertEquals(
                "Layout listener should restore the name once room is available again",
                View.VISIBLE,
                appNameView.getVisibility());
    }
}
