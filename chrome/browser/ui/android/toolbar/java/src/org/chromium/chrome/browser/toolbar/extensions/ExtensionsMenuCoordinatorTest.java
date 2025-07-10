// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.toolbar.R;
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
    private ListMenuButton mExtensionsMenuButton;
    @Mock private AnchoredPopupWindow mMenuWindow;
    @Mock private TabCreator mTabCreator;
    @Mock private Tab mTab;

    private ExtensionsMenuCoordinator mExtensionsMenuCoordinator;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        mExtensionsMenuButton = new ListMenuButton(mContext, null);
        mContext.setContentView(mExtensionsMenuButton);
        mCurrentTabSupplier.set(mTab);

        mExtensionsMenuCoordinator =
                new ExtensionsMenuCoordinator(
                        mContext,
                        mExtensionsMenuButton,
                        mCurrentTabSupplier,
                        mTabCreator,
                        mMenuWindow);
    }

    @Test
    public void testShowMenu() {
        mExtensionsMenuCoordinator.showMenu();
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
