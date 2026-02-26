// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuBridgeJni;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuTypes;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridgeRule;
import org.chromium.chrome.browser.ui.extensions.RequestAccessButtonParams;
import org.chromium.ui.listmenu.ListMenuButton;

/** Unit tests for ExtensionsMenuAndAccessControlButtonCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionsMenuAndAccessControlButtonCoordinatorTest {
    private static final long BROWSER_WINDOW_POINTER = 1000L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final FakeExtensionActionsBridgeRule mBridgeRule = new FakeExtensionActionsBridgeRule();

    @Mock private ListMenuButton mExtensionsMenuButton;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private ChromeAndroidTask mTask;
    @Mock private Profile mProfile;
    @Mock private TabCreator mTabCreator;
    @Mock private ExtensionsToolbarBridge mExtensionsToolbarBridge;
    @Mock private View mRequestAccessButton;
    @Mock private ExtensionsMenuBridge.Natives mExtensionsMenuBridgeJniMock;

    private Activity mContext;
    private ExtensionsMenuAndAccessControlButtonCoordinator mCoordinator;

    @Before
    public void setUp() {
        AppCompatActivity activity =
                Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContext = activity;

        ExtensionsMenuBridgeJni.setInstanceForTesting(mExtensionsMenuBridgeJniMock);
        when(mExtensionsMenuBridgeJniMock.init(any(), anyLong())).thenReturn(1L);
        ExtensionsMenuTypes.ControlState toggleState =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.ENABLED,
                        "toggle_text",
                        "accessible_name",
                        "tooltip",
                        true,
                        /* icon= */ null);
        ExtensionsMenuTypes.SiteSettingsState siteSettingsState =
                new ExtensionsMenuTypes.SiteSettingsState("label", false, toggleState);
        when(mExtensionsMenuBridgeJniMock.getSiteSettings(anyLong())).thenReturn(siteSettingsState);

        when(mExtensionsToolbarBridge.getRequestAccessButtonParams(any()))
                .thenReturn(new RequestAccessButtonParams(new String[0], ""));
        View mockRootView = mock(View.class);
        when(mExtensionsMenuButton.getRootView()).thenReturn(mockRootView);

        when(mTask.getOrCreateNativeBrowserWindowPtr(mProfile)).thenReturn(BROWSER_WINDOW_POINTER);
        when(mExtensionsToolbarBridge.getExtensionsMenuButtonState(any()))
                .thenReturn(ExtensionsToolbarBridge.ExtensionsMenuButtonState.DEFAULT);
        mBridgeRule.getFakeBridge().getOrCreateTaskModel(mTask, mProfile).setInitialized(true);

        mCoordinator =
                new ExtensionsMenuAndAccessControlButtonCoordinator(
                        mContext,
                        mExtensionsMenuButton,
                        mThemeColorProvider,
                        mTask,
                        mProfile,
                        ObservableSuppliers.createNullable(),
                        mTabCreator,
                        mExtensionsToolbarBridge,
                        mRequestAccessButton);
    }

    @Test
    public void testDestroy() {
        mCoordinator.destroy();
        // Verifying that the bridge's observer is removed, which happens in the sub-coordinator's
        // mediator.
        verify(mExtensionsToolbarBridge).removeObserver(any());
    }
}
