// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Looper;

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
import org.robolectric.Shadows;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuBridgeJni;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge.TaskModel;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridgeRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuHost;

/** Unit tests for ExtensionsMenuCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionsMenuCoordinatorTest {
    private static final long BROWSER_WINDOW_POINTER = 1000L;
    private static final long EXTENSIONS_MENU_BRIDGE_POINTER = 10001L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

    private Activity mContext;

    private final SettableNullableObservableSupplier<Tab> mCurrentTabSupplier =
            ObservableSuppliers.createNullable();
    private ListMenuButton mExtensionsMenuButton;
    @Mock private TabCreator mTabCreator;
    @Mock private ChromeAndroidTask mTask;
    @Mock private Profile mProfile;
    @Mock private Tab mTab;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private ExtensionsMenuBridge.Natives mExtensionsMenuBridgeJniMock;

    @Rule
    public final FakeExtensionActionsBridgeRule mBridgeRule = new FakeExtensionActionsBridgeRule();

    private final FakeExtensionActionsBridge mBridge = mBridgeRule.getFakeBridge();
    private TaskModel mTaskModel;

    private ExtensionsMenuCoordinator mExtensionsMenuCoordinator;

    @Before
    public void setUp() {
        AppCompatActivity activity =
                Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContext = activity;

        mExtensionsMenuButton = new ListMenuButton(activity, null);
        activity.setContentView(mExtensionsMenuButton);

        when(mTask.getOrCreateNativeBrowserWindowPtr()).thenReturn(BROWSER_WINDOW_POINTER);
        when(mTab.getProfile()).thenReturn(mProfile);

        mTaskModel = mBridge.getOrCreateTaskModel(mTask);
        mTaskModel.setInitialized(true);

        // Mock {@link ExtensionsMenuBridge}.
        ExtensionsMenuBridgeJni.setInstanceForTesting(mExtensionsMenuBridgeJniMock);
        when(mExtensionsMenuBridgeJniMock.init(Mockito.any(), Mockito.anyLong()))
                .thenReturn(EXTENSIONS_MENU_BRIDGE_POINTER);

        mExtensionsMenuCoordinator =
                new ExtensionsMenuCoordinator(
                        mContext,
                        mExtensionsMenuButton,
                        mThemeColorProvider,
                        mTask,
                        mCurrentTabSupplier,
                        mTabCreator);

        // Ensure the tab / profile suppliers have triggered their initial callbacks.
        Shadows.shadowOf(Looper.getMainLooper()).idle();
    }

    @After
    public void tearDown() {
        mExtensionsMenuCoordinator.destroy();
    }

    @Test
    public void testShowMenu_showsImmediately() {
        // Set the current tab.
        mCurrentTabSupplier.set(mTab);

        ListMenuHost.PopupMenuShownListener shownListener =
                Mockito.mock(ListMenuHost.PopupMenuShownListener.class);
        mExtensionsMenuButton.addPopupListener(shownListener);

        // Click on the button. The data is ready, so the menu should appear immediately.
        mExtensionsMenuButton.performClick();
        verify(shownListener).onPopupMenuShown();
    }

    // TODO(crbug.com/473213114): enable this test once we show the extensions menu after the
    // data is ready.
    @DisabledTest
    public void testShowMenu_showsAfterDataReady() {
        ListMenuHost.PopupMenuShownListener shownListener =
                Mockito.mock(ListMenuHost.PopupMenuShownListener.class);
        mExtensionsMenuButton.addPopupListener(shownListener);

        // Click on the button. At this point, the data is not ready, so the menu should not appear.
        mExtensionsMenuButton.performClick();
        verify(shownListener, never()).onPopupMenuShown();

        // Set the current tab to trigger the data ready callback.
        mCurrentTabSupplier.set(mTab);

        // The callback should have triggered the menu to finally show.
        verify(shownListener).onPopupMenuShown();
    }

    @Test
    public void testCloseMenu() {
        mCurrentTabSupplier.set(mTab);

        ListMenuHost.PopupMenuShownListener shownListener =
                Mockito.mock(ListMenuHost.PopupMenuShownListener.class);
        mExtensionsMenuButton.addPopupListener(shownListener);
        mExtensionsMenuButton.performClick();
        verify(shownListener).onPopupMenuShown();

        mExtensionsMenuCoordinator
                .getContentView()
                .findViewById(R.id.extensions_menu_close_button)
                .performClick();

        verify(shownListener).onPopupMenuDismissed();
    }

    @Test
    public void testManageExtensions() {
        mCurrentTabSupplier.set(mTab);

        ListMenuHost.PopupMenuShownListener shownListener =
                Mockito.mock(ListMenuHost.PopupMenuShownListener.class);
        mExtensionsMenuButton.addPopupListener(shownListener);
        mExtensionsMenuButton.performClick();
        verify(shownListener).onPopupMenuShown();

        mExtensionsMenuCoordinator
                .getContentView()
                .findViewById(R.id.extensions_menu_manage_extensions_button)
                .performClick();
        verify(shownListener).onPopupMenuDismissed();
        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(UrlConstants.CHROME_EXTENSIONS_URL, mLoadUrlParamsCaptor.getValue().getUrl());
    }

    @Test
    public void testDiscoverExtensions() {
        mCurrentTabSupplier.set(mTab);

        ListMenuHost.PopupMenuShownListener shownListener =
                Mockito.mock(ListMenuHost.PopupMenuShownListener.class);
        mExtensionsMenuButton.addPopupListener(shownListener);
        mExtensionsMenuButton.performClick();
        verify(shownListener).onPopupMenuShown();

        mExtensionsMenuCoordinator
                .getContentView()
                .findViewById(R.id.extensions_menu_discover_extensions_button)
                .performClick();
        verify(shownListener).onPopupMenuDismissed();
        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(UrlConstants.CHROME_WEBSTORE_URL, mLoadUrlParamsCaptor.getValue().getUrl());
    }

    @Test
    public void testMediatorLifetime() {
        mCurrentTabSupplier.set(mTab);

        // Mediator should be null before the menu is opened.
        assertNull(mExtensionsMenuCoordinator.mMediator);

        // Open the menu.
        ListMenuHost.PopupMenuShownListener shownListener =
                Mockito.mock(ListMenuHost.PopupMenuShownListener.class);
        mExtensionsMenuButton.addPopupListener(shownListener);
        mExtensionsMenuButton.performClick();
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
}
