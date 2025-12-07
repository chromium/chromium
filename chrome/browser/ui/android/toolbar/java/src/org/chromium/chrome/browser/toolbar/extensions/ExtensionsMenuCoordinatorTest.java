// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Looper;

import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.divider.MaterialDivider;

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

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge.ProfileModel;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridgeRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuHost;

/** Unit tests for ExtensionsMenuCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionsMenuCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

    private Activity mContext;

    private final OneshotSupplierImpl<ChromeAndroidTask> mTaskSupplier =
            new OneshotSupplierImpl<>();
    private final ObservableSupplierImpl<@Nullable Profile> mProfileSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<@Nullable Tab> mCurrentTabSupplier =
            new ObservableSupplierImpl<>();
    private ListMenuButton mExtensionsMenuButton;
    private MaterialDivider mExtensionsMenuTabSwitcherDivider;
    @Mock private TabCreator mTabCreator;
    @Mock private ChromeAndroidTask mTask;
    @Mock private Profile mProfile;
    @Mock private Tab mTab;
    @Mock private Tab mAnotherTab;
    @Mock private ThemeColorProvider mThemeColorProvider;

    @Rule
    public final FakeExtensionActionsBridgeRule mBridgeRule = new FakeExtensionActionsBridgeRule();

    private final FakeExtensionActionsBridge mBridge = mBridgeRule.getFakeBridge();
    private ProfileModel mProfileModel;

    private ExtensionsMenuCoordinator mExtensionsMenuCoordinator;

    @Before
    public void setUp() {
        AppCompatActivity activity =
                Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContext = activity;

        mExtensionsMenuButton = new ListMenuButton(activity, null);
        mExtensionsMenuTabSwitcherDivider = new MaterialDivider(activity);
        activity.setContentView(mExtensionsMenuButton);

        mTaskSupplier.set(mTask);

        mProfileModel = mBridge.getOrCreateProfileModel(mProfile);
        mProfileModel.setInitialized(true);

        mCurrentTabSupplier.set(mTab);
        when(mTab.getProfile()).thenReturn(mProfile);

        mProfileSupplier.set(mProfile);

        mExtensionsMenuCoordinator =
                new ExtensionsMenuCoordinator(
                        mContext,
                        mExtensionsMenuButton,
                        mExtensionsMenuTabSwitcherDivider,
                        mThemeColorProvider,
                        mTaskSupplier,
                        mProfileSupplier,
                        mCurrentTabSupplier,
                        mTabCreator);

        // Ensure the tab / profile suppliers have triggered their initial callbacks.
        Shadows.shadowOf(Looper.getMainLooper()).idle();
    }

    @Test
    public void testShowMenu_showsAfterDataReady() {
        if (mExtensionsMenuCoordinator != null) mExtensionsMenuCoordinator.destroy();

        mProfileSupplier.set(null);
        mCurrentTabSupplier.set(mAnotherTab);

        mExtensionsMenuCoordinator =
                new ExtensionsMenuCoordinator(
                        mContext,
                        mExtensionsMenuButton,
                        mExtensionsMenuTabSwitcherDivider,
                        mThemeColorProvider,
                        mTaskSupplier,
                        mProfileSupplier,
                        mCurrentTabSupplier,
                        mTabCreator);

        ListMenuHost.PopupMenuShownListener shownListener =
                Mockito.mock(ListMenuHost.PopupMenuShownListener.class);
        mExtensionsMenuButton.addPopupListener(shownListener);

        // Click on the button. At this point, the data is not ready, so the menu should not appear.
        mExtensionsMenuButton.performClick();
        verify(shownListener, never()).onPopupMenuShown();

        // Simulate callbacks that happen after the construction of the mediator by manually setting
        // new variables.
        mProfileSupplier.set(mProfile);
        mCurrentTabSupplier.set(mTab);
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        // The callback should have triggered the menu to finally show.
        verify(shownListener).onPopupMenuShown();
    }

    @Test
    public void testCloseMenu() {
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
}
