// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionTestUtils;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuBridgeJni;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuTypes;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuHost;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for ExtensionsMenuCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionsMenuCoordinatorTest {
    // Constants identifying the tabs and pointers used in the test environment.
    private static final long BROWSER_WINDOW_POINTER = 1000L;
    private static final long EXTENSIONS_MENU_BRIDGE_POINTER = 10001L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TabCreator mTabCreator;
    @Mock private ChromeAndroidTask mTask;
    @Mock private Tab mTab;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private ExtensionsToolbarBridge mExtensionsToolbarBridge;
    @Mock private ExtensionsMenuBridge.Natives mExtensionsMenuBridgeJniMock;

    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

    private Activity mContext;
    private final SettableNullableObservableSupplier<Tab> mCurrentTabSupplier =
            ObservableSuppliers.createNullable();
    private ListMenuButton mExtensionsMenuButton;

    private ExtensionsMenuCoordinator mExtensionsMenuCoordinator;
    private ExtensionsMenuBridge mCapturedMenuBridge;

    private ExtensionsMenuTypes.SiteSettingsState mSiteSettingsState;

    @Before
    public void setUp() {
        ExtensionsMenuBridgeJni.setInstanceForTesting(mExtensionsMenuBridgeJniMock);
        when(mExtensionsMenuBridgeJniMock.init(Mockito.any(), Mockito.anyLong())).thenReturn(1L);

        AppCompatActivity activity =
                Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContext = activity;

        mExtensionsMenuButton = new ListMenuButton(activity, null);
        activity.setContentView(mExtensionsMenuButton);

        when(mTask.getOrCreateNativeBrowserWindowPtr(mProfile)).thenReturn(BROWSER_WINDOW_POINTER);
        when(mTab.getProfile()).thenReturn(mProfile);

        // Mock {@link ExtensionsMenuBridge}.
        ExtensionsMenuBridgeJni.setInstanceForTesting(mExtensionsMenuBridgeJniMock);
        // Capture the bridge instance when it is initialized. This allows us to trigger
        // callbacks (like onReady) on the specific bridge instance created by the
        // Coordinator/Mediator, even though it is created lazily after setUp() finishes.
        doAnswer(
                        invocation -> {
                            mCapturedMenuBridge = invocation.getArgument(0);
                            return EXTENSIONS_MENU_BRIDGE_POINTER;
                        })
                .when(mExtensionsMenuBridgeJniMock)
                .init(any(), anyLong());

        // Default to not ready, so we can test the waiting logic.
        when(mExtensionsMenuBridgeJniMock.isReady(anyLong())).thenReturn(false);

        // Set the current tab.
        mCurrentTabSupplier.set(mTab);
        when(mExtensionsToolbarBridge.getExtensionsMenuButtonState(any()))
                .thenReturn(ExtensionsToolbarBridge.ExtensionsMenuButtonState.DEFAULT);

        mExtensionsMenuCoordinator =
                new ExtensionsMenuCoordinator(
                        mContext,
                        mExtensionsMenuButton,
                        mThemeColorProvider,
                        mTask,
                        mProfile,
                        mCurrentTabSupplier,
                        mTabCreator,
                        mExtensionsToolbarBridge);

        // Clear invocations from initialization to ensure tests start fresh.
        Mockito.clearInvocations(mExtensionsMenuBridgeJniMock);
    }

    @After
    public void tearDown() {
        mExtensionsMenuCoordinator.destroy();
    }

    /**
     * Tests that the extensions menu is shown when the menu button is clicked and actions are
     * initialized.
     */
    @Test
    public void testShowMenu() {
        ListMenuHost.PopupMenuShownListener shownListener =
                Mockito.mock(ListMenuHost.PopupMenuShownListener.class);
        mExtensionsMenuButton.addPopupListener(shownListener);

        // Click on the button. The menu should not be shown yet, but the mediator should be
        // created.
        mExtensionsMenuButton.performClick();
        verify(shownListener, never()).onPopupMenuShown();
        assertNotNull(mExtensionsMenuCoordinator.mMediator);

        // Menu should be shown once mediator trigger the onReady runnable.
        triggerOnMediatorReady();
        verify(shownListener).onPopupMenuShown();
    }

    /** Tests that the extensions menu can be dismissed by clicking the close button. */
    @Test
    public void testCloseMenu() {
        // Show the menu.
        ListMenuHost.PopupMenuShownListener shownListener =
                Mockito.mock(ListMenuHost.PopupMenuShownListener.class);
        mExtensionsMenuButton.addPopupListener(shownListener);
        mExtensionsMenuButton.performClick();
        triggerOnMediatorReady();
        verify(shownListener).onPopupMenuShown();

        mExtensionsMenuCoordinator
                .getContentView()
                .findViewById(R.id.extensions_menu_close_button)
                .performClick();

        verify(shownListener).onPopupMenuDismissed();
    }

    /** Tests that clicking "Manage extensions" opens the extensions management page. */
    @Test
    public void testManageExtensions() {
        // Show the menu.
        ListMenuHost.PopupMenuShownListener shownListener =
                Mockito.mock(ListMenuHost.PopupMenuShownListener.class);
        mExtensionsMenuButton.addPopupListener(shownListener);
        mExtensionsMenuButton.performClick();
        triggerOnMediatorReady();
        verify(shownListener).onPopupMenuShown();

        // Click on the manage extensions button.
        mExtensionsMenuCoordinator
                .getContentView()
                .findViewById(R.id.extensions_menu_manage_extensions_button)
                .performClick();

        // Verify that the menu is closed and the tab is loaded with the correct URL.
        verify(shownListener).onPopupMenuDismissed();
        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(UrlConstants.CHROME_EXTENSIONS_URL, mLoadUrlParamsCaptor.getValue().getUrl());
    }

    /** Tests that clicking the discover extensions button opens the web store page. */
    @Test
    public void testDiscoverExtensions() {
        // Show the menu.
        ListMenuHost.PopupMenuShownListener shownListener =
                Mockito.mock(ListMenuHost.PopupMenuShownListener.class);
        mExtensionsMenuButton.addPopupListener(shownListener);
        mExtensionsMenuButton.performClick();
        triggerOnMediatorReady();
        verify(shownListener).onPopupMenuShown();

        // Click on the discover extensions button.
        mExtensionsMenuCoordinator
                .getContentView()
                .findViewById(R.id.extensions_menu_discover_extensions_button)
                .performClick();

        // Verify that the menu is closed and the tab is loaded with the correct URL.
        verify(shownListener).onPopupMenuDismissed();
        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(UrlConstants.CHROME_WEBSTORE_URL, mLoadUrlParamsCaptor.getValue().getUrl());
    }

    /**
     * Tests the lifetime of the {@link ExtensionsMenuMediator}. It should be created when the menu
     * opens and destroyed when the menu closes.
     */
    @Test
    public void testMediatorLifetime() {
        // Mediator should be null before the menu is opened.
        assertNull(mExtensionsMenuCoordinator.mMediator);

        // Open the menu.
        ListMenuHost.PopupMenuShownListener shownListener =
                Mockito.mock(ListMenuHost.PopupMenuShownListener.class);
        mExtensionsMenuButton.addPopupListener(shownListener);
        mExtensionsMenuButton.performClick();
        triggerOnMediatorReady();
        verify(shownListener).onPopupMenuShown();

        // Mediator should be created when the menu is opened.
        assertNotNull(mExtensionsMenuCoordinator.mMediator);

        // Close the menu.
        mExtensionsMenuCoordinator
                .getContentView()
                .findViewById(R.id.extensions_menu_close_button)
                .performClick();

        verify(shownListener).onPopupMenuDismissed();

        // Mediator should be destroyed when the menu is closed.
        assertNull(mExtensionsMenuCoordinator.mMediator);
    }

    /**
     * Helper to simulate the callback from the native layer indicating that extension data is
     * ready. This is required because the menu only shows itself after this data is received.
     */
    private void triggerOnMediatorReady() {
        // We must mock a non-null return value for getMenuEntries() and getSiteSettings() because
        // the Mediator will immediately call this method upon receiving the onReady signal.
        List<ExtensionsMenuTypes.MenuEntryState> mockEntries = new ArrayList<>();
        mockEntries.add(
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_a",
                        "Extension A",
                        ExtensionTestUtils.createSimpleIcon(Color.RED),
                        /* isPinned= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(mockEntries);
        mSiteSettingsState = createSiteSettingsState("label", true);
        when(mExtensionsMenuBridgeJniMock.getSiteSettings(anyLong()))
                .thenReturn(mSiteSettingsState);

        // This simulates the actual C++ -> Java call (Observer.onReady) that happens when
        // the native model is initialized.
        assertNotNull("Bridge should have been initialized by the click", mCapturedMenuBridge);
        mCapturedMenuBridge.onReady();
    }

    @Test
    public void testUpdateSiteSettingsToggle() {
        mExtensionsMenuButton.performClick();
        MaterialSwitchWithText toggle =
                mExtensionsMenuCoordinator
                        .getContentView()
                        .findViewById(R.id.extensions_menu_site_settings_toggle);

        mExtensionsMenuCoordinator.mMediator.updateSiteSettingsToggle(
                createSiteSettingsState("label", true));
        assertEquals(View.VISIBLE, toggle.getVisibility());
        assertEquals(true, toggle.isChecked());
        assertEquals("label", toggle.getText());

        mExtensionsMenuCoordinator.mMediator.updateSiteSettingsToggle(
                createSiteSettingsState(
                        "label_2", false, ExtensionsMenuTypes.ControlState.Status.HIDDEN));
        assertEquals(View.GONE, toggle.getVisibility());
        assertEquals(false, toggle.isChecked());
        assertEquals("label_2", toggle.getText());
    }

    @Test
    public void testSiteSettingsToggle_ClickCallsBridge() {
        mCurrentTabSupplier.set(mTab);
        mExtensionsMenuButton.performClick();

        MaterialSwitchWithText toggle =
                mExtensionsMenuCoordinator
                        .getContentView()
                        .findViewById(R.id.extensions_menu_site_settings_toggle);

        // Click to toggle from checked (default) to unchecked.
        toggle.performClick();
        verify(mExtensionsMenuBridgeJniMock, Mockito.times(1))
                .onSiteSettingsToggleChanged(Mockito.anyLong(), Mockito.eq(false));

        // Click again to toggle back to checked.
        toggle.performClick();
        verify(mExtensionsMenuBridgeJniMock, Mockito.times(1))
                .onSiteSettingsToggleChanged(Mockito.anyLong(), Mockito.eq(true));
    }

    private ExtensionsMenuTypes.SiteSettingsState createSiteSettingsState(
            String label, boolean isOn) {
        return createSiteSettingsState(
                label, isOn, ExtensionsMenuTypes.ControlState.Status.ENABLED);
    }

    private ExtensionsMenuTypes.SiteSettingsState createSiteSettingsState(
            String label, boolean isOn, @ExtensionsMenuTypes.ControlState.Status int status) {
        ExtensionsMenuTypes.ControlState toggleState =
                new ExtensionsMenuTypes.ControlState(
                        status,
                        "toggle_text",
                        "accessible_name",
                        "tooltip",
                        isOn,
                        /* icon= */ null);
        return new ExtensionsMenuTypes.SiteSettingsState(
                label, /* hasTooltip= */ false, toggleState);
    }
}
