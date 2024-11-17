// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.menu_button;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.res.Resources;
import android.widget.ImageButton;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.CallbackUtils;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.top.ToolbarTablet;
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
    @Mock private BrowserStateBrowserControlsVisibilityDelegate mControlsVisibilityDelegate;
    @Mock private Activity mActivityMock;
    @Mock private MenuButtonCoordinator.SetFocusFunction mFocusFunction;
    @Mock private AppMenuCoordinator mAppMenuCoordinator;
    @Mock private AppMenuHandler mAppMenuHandler;
    @Mock private AppMenuButtonHelper mAppMenuButtonHelper;
    MenuButton mMenuButton;
    @Mock ImageButton mImageButton;
    @Mock private AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    @Mock private Runnable mRequestRenderRunnable;
    @Mock ThemeColorProvider mThemeColorProvider;
    @Mock Resources mResources;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private KeyboardVisibilityDelegate mKeyboardDelegate;

    private MenuUiState mMenuUiState;
    private OneshotSupplierImpl<AppMenuCoordinator> mAppMenuSupplier;
    private MenuButtonCoordinator mMenuButtonCoordinator;
    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Get menu button.
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        ToolbarTablet toolbarTablet =
                (ToolbarTablet)
                        mActivity.getLayoutInflater().inflate(R.layout.toolbar_tablet, null);
        mMenuButton = spy(toolbarTablet.findViewById(R.id.menu_button_wrapper));

        doReturn(mAppMenuHandler).when(mAppMenuCoordinator).getAppMenuHandler();
        doReturn(mAppMenuButtonHelper).when(mAppMenuHandler).createAppMenuButtonHelper();
        doReturn(mAppMenuPropertiesDelegate)
                .when(mAppMenuCoordinator)
                .getAppMenuPropertiesDelegate();
        mAppMenuSupplier = new OneshotSupplierImpl<>();
        mMenuUiState = new MenuUiState();
        doReturn(mMenuButton).when(mActivityMock).findViewById(R.id.menu_button_wrapper);
        doReturn(mImageButton).when(mMenuButton).getImageButton();
        doReturn(mResources).when(mActivityMock).getResources();
        doReturn(10)
                .when(mResources)
                .getDimensionPixelSize(R.dimen.toolbar_url_focus_translation_x);
        doReturn(mActivity.getResources().getString(R.string.accessibility_toolbar_btn_menu))
                .when(mResources)
                .getString(R.string.accessibility_toolbar_btn_menu);
        doReturn(new WeakReference<>(mActivityMock)).when(mWindowAndroid).getActivity();
        doReturn(mKeyboardDelegate).when(mWindowAndroid).getKeyboardDelegate();

        mMenuButtonCoordinator =
                new MenuButtonCoordinator(
                        mAppMenuSupplier,
                        mControlsVisibilityDelegate,
                        mWindowAndroid,
                        mFocusFunction,
                        mRequestRenderRunnable,
                        true,
                        () -> false,
                        mThemeColorProvider,
                        () -> null,
                        CallbackUtils.emptyRunnable(),
                        R.id.menu_button_wrapper);
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
    public void testHoverTooltipText() {
        Assert.assertEquals(
                "Tooltip text for Menu button is not as expected",
                mActivity.getResources().getString(R.string.accessibility_toolbar_btn_menu),
                mMenuButton.getTooltipText());
    }
}
