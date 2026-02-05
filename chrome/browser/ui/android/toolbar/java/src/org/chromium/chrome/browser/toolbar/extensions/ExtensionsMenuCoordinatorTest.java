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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuBridgeJni;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuHost;

/** Unit tests for ExtensionsMenuCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionsMenuCoordinatorTest {
    // Constants identifying the tabs and pointers used in the test environment.
    private static final long BROWSER_WINDOW_POINTER = 1000L;
    private static final long EXTENSIONS_MENU_BRIDGE_POINTER = 10001L;

    // Pre-defined simple icons for verifying icon assignment in the model.
    // private static final Bitmap ICON_RED = createSimpleIcon(Color.RED);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabCreator mTabCreator;
    @Mock private ChromeAndroidTask mTask;
    @Mock private Tab mTab;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private ExtensionsMenuBridge.Natives mExtensionsMenuBridgeJniMock;

    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

    private Activity mContext;
    private final SettableNullableObservableSupplier<Tab> mCurrentTabSupplier =
            ObservableSuppliers.createNullable();
    private ListMenuButton mExtensionsMenuButton;

    private ExtensionsMenuCoordinator mExtensionsMenuCoordinator;
    private ExtensionsMenuBridge mCapturedMenuBridge;

    @Before
    public void setUp() {
        AppCompatActivity activity =
                Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContext = activity;

        mExtensionsMenuButton = new ListMenuButton(activity, null);
        activity.setContentView(mExtensionsMenuButton);

        when(mTask.getOrCreateNativeBrowserWindowPtr()).thenReturn(BROWSER_WINDOW_POINTER);

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

        mExtensionsMenuCoordinator =
                new ExtensionsMenuCoordinator(
                        mContext,
                        mExtensionsMenuButton,
                        mThemeColorProvider,
                        mTask,
                        mCurrentTabSupplier,
                        mTabCreator);
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
        // We must mock a non-null return value for getActions() because the Mediator will
        // immediately call this method upon receiving the onReady signal.
        String[] mockActions = new String[] {"extA", "Extension A"};
        when(mExtensionsMenuBridgeJniMock.getActions(anyLong())).thenReturn(mockActions);

        // This simulates the actual C++ -> Java call (Observer.onReady) that happens when
        // the native model is initialized.
        assertNotNull("Bridge should have been initialized by the click", mCapturedMenuBridge);
        mCapturedMenuBridge.onReady();
    }
}
