// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.menu_button;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.widget.ImageButton;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for ToolbarAppMenuManager. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class MenuButtonCoordinatorTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BrowserStateBrowserControlsVisibilityDelegate mControlsVisibilityDelegate;
    @Mock private Activity mActivity;
    @Mock private MenuButtonCoordinator.SetFocusFunction mFocusFunction;
    @Mock private AppMenuCoordinator mAppMenuCoordinator;
    @Mock private AppMenuHandler mAppMenuHandler;
    @Mock private AppMenuButtonHelper mAppMenuButtonHelper;
    @Mock MenuButton mMenuButton;
    @Mock ImageButton mImageButton;
    @Mock private AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    @Mock private Runnable mRequestRenderRunnable;
    @Mock ThemeColorProvider mThemeColorProvider;
    @Mock IncognitoStateProvider mIncognitoStateProvider;
    @Mock Resources mResources;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private KeyboardVisibilityDelegate mKeyboardDelegate;
    @Mock private MenuButtonCoordinator.VisibilityDelegate mVisibilityDelegate;

    private MenuUiState mMenuUiState;
    private OneshotSupplierImpl<AppMenuCoordinator> mAppMenuSupplier;
    private MenuButtonCoordinator mMenuButtonCoordinator;

    @Before
    public void setUp() {
        doReturn(mAppMenuHandler).when(mAppMenuCoordinator).getAppMenuHandler();
        doReturn(mAppMenuButtonHelper).when(mAppMenuHandler).createAppMenuButtonHelper();
        doReturn(mAppMenuPropertiesDelegate)
                .when(mAppMenuCoordinator)
                .getAppMenuPropertiesDelegate();
        mAppMenuSupplier = new OneshotSupplierImpl<>();
        mMenuUiState = new MenuUiState();
        doReturn(mMenuButton).when(mActivity).findViewById(R.id.menu_button_wrapper);
        doReturn(mImageButton).when(mMenuButton).getImageButton();
        doReturn(mResources).when(mActivity).getResources();
        doReturn(10)
                .when(mResources)
                .getDimensionPixelSize(R.dimen.toolbar_url_focus_translation_x);
        doReturn(new WeakReference<>(mActivity)).when(mWindowAndroid).getActivity();
        doReturn(mKeyboardDelegate).when(mWindowAndroid).getKeyboardDelegate();

        initMenuButtonCoordinator(null);
    }

    @Test
    public void testEnterKeyPress() {
        mAppMenuSupplier.set(mAppMenuCoordinator);

        mMenuButtonCoordinator.onEnterKeyPress();
        verify(mAppMenuButtonHelper).onEnterKeyPress(mImageButton);

        mMenuButtonCoordinator.destroy();
        mMenuButtonCoordinator.onEnterKeyPress();
        verify(mAppMenuButtonHelper, times(1)).onEnterKeyPress(mImageButton);
    }

    @Test
    public void testSetHighlight() {
        mAppMenuSupplier.set(mAppMenuCoordinator);

        mMenuButtonCoordinator.highlightMenuItemOnShow(R.id.close_all_tabs_menu_id);
        verify(mAppMenuButtonHelper).highlightMenuItemOnShow(R.id.close_all_tabs_menu_id);
    }

    @Test
    public void testVisibilityDelegate_isVisible() {
        mVisibilityDelegate = Mockito.mock(MenuButtonCoordinator.VisibilityDelegate.class);
        initMenuButtonCoordinator(mVisibilityDelegate);

        when(mVisibilityDelegate.isMenuButtonVisible()).thenReturn(true);
        assertTrue(
                "Should return visibility from VisibilityDelegate",
                mMenuButtonCoordinator.isVisible());

        when(mVisibilityDelegate.isMenuButtonVisible()).thenReturn(false);
        assertFalse(
                "Should return visibility from VisibilityDelegate",
                mMenuButtonCoordinator.isVisible());
    }

    @Test
    public void testVisibilityDelegate_disable() {
        mVisibilityDelegate = Mockito.mock(MenuButtonCoordinator.VisibilityDelegate.class);
        initMenuButtonCoordinator(mVisibilityDelegate);

        mMenuButtonCoordinator.disableMenuButton();
        verify(mVisibilityDelegate).setMenuButtonVisible(false);
    }

    private void initMenuButtonCoordinator(
            MenuButtonCoordinator.VisibilityDelegate visibilityDelegate) {
        // clang-format off
        mMenuButtonCoordinator =
                new MenuButtonCoordinator(
                        mActivity,
                        mAppMenuSupplier,
                        mControlsVisibilityDelegate,
                        mWindowAndroid,
                        mFocusFunction,
                        mRequestRenderRunnable,
                        true,
                        () -> false,
                        mThemeColorProvider,
                        mIncognitoStateProvider,
                        () -> null,
                        () -> {},
                        R.id.menu_button_wrapper,
                        visibilityDelegate,
                        /* isWebApp= */ false);
        // clang-format on
    }
}
