// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge.ProfileModel;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridgeRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.widget.AnchoredPopupWindow;

/** Unit tests for ExtensionsMenuCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionsMenuCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

    private Activity mContext;

    private final ObservableSupplierImpl<Tab> mCurrentTabSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();
    private ListMenuButton mExtensionsMenuButton;
    @Mock private AnchoredPopupWindow mMenuWindow;
    @Mock private TabCreator mTabCreator;
    @Mock private Profile mProfile;
    @Mock private Tab mTab;
    @Mock private Profile mAnotherProfile;
    @Mock private Tab mAnotherTab;

    @Rule
    public final FakeExtensionActionsBridgeRule mBridgeRule = new FakeExtensionActionsBridgeRule();

    private final FakeExtensionActionsBridge mBridge = mBridgeRule.getFakeBridge();
    private ProfileModel mProfileModel;

    private ExtensionsMenuCoordinator mExtensionsMenuCoordinator;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        mExtensionsMenuButton = new ListMenuButton(mContext, null);
        mContext.setContentView(mExtensionsMenuButton);

        mProfileModel = mBridge.getOrCreateProfileModel(mProfile);
        mProfileModel.setInitialized(true);

        mCurrentTabSupplier.set(mTab);
        when(mTab.getProfile()).thenReturn(mProfile);

        mProfileSupplier.set(mProfile);

        mExtensionsMenuCoordinator =
                new ExtensionsMenuCoordinator(
                        mContext,
                        mExtensionsMenuButton,
                        mProfileSupplier,
                        mCurrentTabSupplier,
                        mTabCreator,
                        mMenuWindow);
    }

    @Test
    public void testShowMenu_showsAfterDataReady() {
        // Prepare the suppliers for the simulation of the construction of the mediator. We need to
        // do this only for this test because `SetUp` sets default values to these suppliers. Since
        // setting the tab to `null` does nothing, we set it to a temporary mock tab here.
        mProfileSupplier.set(null);
        mCurrentTabSupplier.set(mAnotherTab);

        // Call showMenu(). At this point, the data is not ready, so the menu should not appear.
        mExtensionsMenuCoordinator.showMenu();
        verify(mMenuWindow, never()).show();

        // Simulate callbacks that happen after the construction of the mediator by manually setting
        // new variables.
        mProfileSupplier.set(mProfile);
        mCurrentTabSupplier.set(mTab);

        // The callback should have triggered the menu to finally show.
        verify(mMenuWindow).show();
    }

    @Test
    public void testCloseMenu() {
        mExtensionsMenuCoordinator
                .getContentView()
                .findViewById(R.id.extensions_menu_close_button)
                .performClick();
        verify(mMenuWindow).dismiss();
    }

    @Test
    public void testManageExtensions() {
        mExtensionsMenuCoordinator
                .getContentView()
                .findViewById(R.id.extensions_menu_manage_extensions_button)
                .performClick();
        verify(mMenuWindow).dismiss();
        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(UrlConstants.CHROME_EXTENSIONS_URL, mLoadUrlParamsCaptor.getValue().getUrl());
    }

    @Test
    public void testDiscoverExtensions() {
        mExtensionsMenuCoordinator
                .getContentView()
                .findViewById(R.id.extensions_menu_discover_extensions_button)
                .performClick();
        verify(mMenuWindow).dismiss();
        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(UrlConstants.CHROME_WEBSTORE_URL, mLoadUrlParamsCaptor.getValue().getUrl());
    }
}
