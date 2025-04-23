// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Build;
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

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup;
import org.chromium.chrome.browser.web_app_header.R;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.TestActivity;

@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
@Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
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
    @Mock public NavigationPopup.HistoryDelegate mHistoryDelegate;
    @Mock public WebappExtras mWebAppExtras;
    @Mock public Tab mTab;

    private WebAppHeaderLayoutCoordinator mCoordinator;
    private Activity mActivity;
    private ViewGroup mContentView;
    private ViewStub mViewStub;
    private ObservableSupplierImpl<Tab> mTabSupplier;
    private AppHeaderState mAppHeaderState;

    @Before
    public void setup() {
        setupStandaloneMode();

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
        mTabSupplier.set(mTab);
        createCoordinator();

        assertEquals(View.VISIBLE, mActivity.findViewById(R.id.refresh_button).getVisibility());
        assertEquals(View.VISIBLE, mActivity.findViewById(R.id.back_button).getVisibility());
    }

    @Test
    public void testReload_shouldReloadTab() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupMinUiMode();
        mTabSupplier.set(mTab);
        when(mTab.isLoading()).thenReturn(false);
        createCoordinator();

        mCoordinator.refreshTab(false);
        verify(mTab).reload();
    }

    @Test
    public void testReloadWhileReloading_shouldStopReloading() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupMinUiMode();
        mTabSupplier.set(mTab);
        when(mTab.isLoading()).thenReturn(true);
        createCoordinator();

        mCoordinator.refreshTab(false);
        verify(mTab).stopLoading();
    }

    @Test
    public void testReloadTabIgnoringCache_shouldReloadIgnoringCache() {
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupMinUiMode();
        mTabSupplier.set(mTab);
        when(mTab.isLoading()).thenReturn(false);
        createCoordinator();

        mCoordinator.refreshTab(true);
        verify(mTab).reloadIgnoringCache();
    }
}
