// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
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

@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
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
        setupStandaloneMode();

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
        mAppHeaderState =
                new AppHeaderState(WINDOW_RECT, WIDEST_UNOCCLUDED_RECT, isInDesktopWindow);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(mAppHeaderState);
    }

    private void setupStandaloneMode() {
        mWebAppExtras =
                new WebappExtras(
                        "",
                        "",
                        "",
                        new WebappIcon(),
                        "",
                        "",
                        DisplayMode.STANDALONE,
                        0,
                        0,
                        0,
                        0,
                        0,
                        false,
                        false,
                        false);

        when(mIntentDataProvider.getWebappExtras()).thenReturn(mWebAppExtras);
        when(mIntentDataProvider.getResolvedDisplayMode()).thenReturn(DisplayMode.STANDALONE);
    }

    private void setupMinUiMode() {
        mWebAppExtras =
                new WebappExtras(
                        "",
                        "",
                        "",
                        new WebappIcon(),
                        "",
                        "",
                        DisplayMode.MINIMAL_UI,
                        0,
                        0,
                        0,
                        0,
                        0,
                        false,
                        false,
                        false);

        when(mIntentDataProvider.getWebappExtras()).thenReturn(mWebAppExtras);
        when(mIntentDataProvider.getResolvedDisplayMode()).thenReturn(DisplayMode.MINIMAL_UI);
    }

    private void setupTab(boolean isLoading, boolean canGoBack) {
        when(mTab.isLoading()).thenReturn(isLoading);
        when(mTab.canGoBack()).thenReturn(canGoBack);
        mTabSupplier.set(mTab);
    }

    private void verifyControlsEnabled() {
        assertTrue(
                "Refresh button should be enabled",
                mActivity.findViewById(R.id.refresh_button).isEnabled());
        assertTrue(
                "Back button should be enabled",
                mActivity.findViewById(R.id.back_button).isEnabled());
    }

    private void verifyControlsDisabled() {
        assertFalse(
                "Refresh button should be disabled",
                mActivity.findViewById(R.id.refresh_button).isEnabled());
        assertFalse(
                "Back button should be disabled",
                mActivity.findViewById(R.id.back_button).isEnabled());
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
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupMinUiMode();
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();

        // Wait for animation to finish and update the view.
        mShadowLooper.idle();

        assertEquals(
                "Reload button should be visible",
                View.VISIBLE,
                mActivity.findViewById(R.id.refresh_button).getVisibility());
        assertEquals(
                "Back button should be visible",
                View.VISIBLE,
                mActivity.findViewById(R.id.back_button).getVisibility());
        assertTrue(
                "Reload button should be enabled",
                mActivity.findViewById(R.id.refresh_button).isEnabled());
        assertFalse(
                "Back button should be disabled",
                mActivity.findViewById(R.id.back_button).isEnabled());
    }

    @Test
    public void testReload_shouldReloadTab() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupMinUiMode();
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();

        mCoordinator.refreshTab(false);
        verify(mTab).reload();
    }

    @Test
    public void testReloadWhileReloading_shouldStopReloading() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupMinUiMode();
        setupTab(/* isLoading= */ true, /* canGoBack= */ false);
        createCoordinator();

        mCoordinator.refreshTab(false);
        verify(mTab).stopLoading();
    }

    @Test
    public void testReloadTabIgnoringCache_shouldReloadIgnoringCache() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupMinUiMode();
        setupTab(/* isLoading= */ false, /* canGoBack= */ false);
        createCoordinator();

        mCoordinator.refreshTab(true);
        verify(mTab).reloadIgnoringCache();
    }

    @Test
    public void testDisableControls() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupMinUiMode();
        setupTab(/* isLoading= */ false, /* canGoBack= */ true);
        createCoordinator();

        mShadowLooper.idle();

        mCoordinator.disableControlsAndClearOldToken(TokenHolder.INVALID_TOKEN);
        verifyControlsDisabled();
    }

    @Test
    public void testEnableControls() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupMinUiMode();
        setupTab(/* isLoading= */ false, /* canGoBack= */ true);
        createCoordinator();

        mShadowLooper.idle();

        int token = mCoordinator.disableControlsAndClearOldToken(TokenHolder.INVALID_TOKEN);
        verifyControlsDisabled();

        mCoordinator.releaseDisabledControlsToken(token);
        verifyControlsEnabled();
    }

    @Test
    public void testDisableControlsOnManyTokens() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupMinUiMode();
        setupTab(/* isLoading= */ false, /* canGoBack= */ true);
        createCoordinator();

        mShadowLooper.idle();

        int firstToken = mCoordinator.disableControlsAndClearOldToken(TokenHolder.INVALID_TOKEN);
        mCoordinator.disableControlsAndClearOldToken(TokenHolder.INVALID_TOKEN);
        verifyControlsDisabled();

        mCoordinator.releaseDisabledControlsToken(firstToken);
        verifyControlsDisabled();
    }
}
