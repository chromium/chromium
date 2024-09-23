// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.menu_button;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.res.Resources;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonProperties.ShowBadgeProperty;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonProperties.ThemeProperty;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

import java.lang.ref.WeakReference;

/** Unit tests for ToolbarAppMenuManager. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class MenuButtonMediatorTest {
    @Mock private BrowserStateBrowserControlsVisibilityDelegate mControlsVisibilityDelegate;
    @Mock private Activity mActivity;
    @Mock private MenuButtonCoordinator.SetFocusFunction mFocusFunction;
    @Mock private AppMenuCoordinator mAppMenuCoordinator;
    @Mock private AppMenuHandler mAppMenuHandler;
    @Mock private AppMenuButtonHelper mAppMenuButtonHelper;
    @Mock private AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    @Mock private Runnable mOnMenuButtonClicked;
    @Mock private Runnable mRequestRenderRunnable;
    @Mock ThemeColorProvider mThemeColorProvider;
    @Mock Resources mResources;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private KeyboardVisibilityDelegate mKeyboardDelegate;
    @Mock private View mUtilityView;

    private MenuUiState mMenuUiState;
    private OneshotSupplierImpl<AppMenuCoordinator> mAppMenuSupplier;
    private PropertyModel mPropertyModel;
    private MenuButtonMediator mMenuButtonMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mPropertyModel =
                new PropertyModel.Builder(MenuButtonProperties.ALL_KEYS)
                        .with(
                                MenuButtonProperties.SHOW_UPDATE_BADGE,
                                new ShowBadgeProperty(false, false))
                        .with(
                                MenuButtonProperties.THEME,
                                new ThemeProperty(
                                        mThemeColorProvider.getTint(),
                                        mThemeColorProvider.getBrandedColorScheme()))
                        .with(MenuButtonProperties.IS_VISIBLE, true)
                        .build();
        doReturn(mAppMenuHandler).when(mAppMenuCoordinator).getAppMenuHandler();
        doReturn(mAppMenuButtonHelper).when(mAppMenuHandler).createAppMenuButtonHelper();
        doReturn(mAppMenuPropertiesDelegate)
                .when(mAppMenuCoordinator)
                .getAppMenuPropertiesDelegate();
        mAppMenuSupplier = new OneshotSupplierImpl<>();
        mMenuUiState = new MenuUiState();
        doReturn(mResources).when(mActivity).getResources();
        doReturn(new WeakReference<>(mActivity)).when(mWindowAndroid).getActivity();
        doReturn(mKeyboardDelegate).when(mWindowAndroid).getKeyboardDelegate();

        mMenuButtonMediator =
                new MenuButtonMediator(
                        mPropertyModel,
                        true,
                        () -> false,
                        mRequestRenderRunnable,
                        mThemeColorProvider,
                        () -> false,
                        mControlsVisibilityDelegate,
                        mFocusFunction,
                        mAppMenuSupplier,
                        mWindowAndroid,
                        () -> mMenuUiState.buttonState,
                        mOnMenuButtonClicked);
    }

    @Test
    public void testInitialization() {
        mAppMenuSupplier.set(mAppMenuCoordinator);
        verify(mAppMenuHandler).addObserver(mMenuButtonMediator);
        verify(mAppMenuHandler).createAppMenuButtonHelper();
    }

    @Test
    public void testAppMenuVisiblityChange_badgeShowing() {
        mAppMenuSupplier.set(mAppMenuCoordinator);
        doReturn(42)
                .when(mControlsVisibilityDelegate)
                .showControlsPersistentAndClearOldToken(TokenHolder.INVALID_TOKEN);
        mPropertyModel.set(
                MenuButtonProperties.SHOW_UPDATE_BADGE, new ShowBadgeProperty(true, false));
        mMenuButtonMediator.onMenuVisibilityChanged(true);

        verify(mFocusFunction).setFocus(false, OmniboxFocusReason.UNFOCUS);
        assertFalse(mPropertyModel.get(MenuButtonProperties.SHOW_UPDATE_BADGE).mShowUpdateBadge);
        verify(mOnMenuButtonClicked).run();

        mMenuButtonMediator.onMenuVisibilityChanged(false);
        verify(mControlsVisibilityDelegate).releasePersistentShowingToken(42);
    }

    @Test
    public void testAppMenuHighlightChange() {
        mAppMenuSupplier.set(mAppMenuCoordinator);

        doReturn(42)
                .when(mControlsVisibilityDelegate)
                .showControlsPersistentAndClearOldToken(TokenHolder.INVALID_TOKEN);

        mMenuButtonMediator.onMenuHighlightChanged(true);
        assertTrue(mPropertyModel.get(MenuButtonProperties.IS_HIGHLIGHTING));

        mMenuButtonMediator.onMenuHighlightChanged(false);
        assertFalse(mPropertyModel.get(MenuButtonProperties.IS_HIGHLIGHTING));
        verify(mControlsVisibilityDelegate).releasePersistentShowingToken(42);
    }

    @Test
    public void testAppMenuUpdateBadge() {
        mAppMenuSupplier.set(mAppMenuCoordinator);

        doReturn(true).when(mActivity).isDestroyed();
        mMenuButtonMediator.updateStateChanged();

        assertFalse(mPropertyModel.get(MenuButtonProperties.SHOW_UPDATE_BADGE).mShowUpdateBadge);
        verify(mRequestRenderRunnable, never()).run();

        doReturn(false).when(mActivity).isDestroyed();
        mMenuButtonMediator.updateStateChanged();

        assertFalse(mPropertyModel.get(MenuButtonProperties.SHOW_UPDATE_BADGE).mShowUpdateBadge);
        verify(mRequestRenderRunnable, never()).run();

        mMenuUiState.buttonState = new MenuButtonState();
        mMenuButtonMediator.updateStateChanged();

        assertTrue(mPropertyModel.get(MenuButtonProperties.SHOW_UPDATE_BADGE).mShowUpdateBadge);
        verify(mRequestRenderRunnable).run();
    }

    @Test
    public void testAppMenuUpdateBadge_activityShouldNotShow() {
        MenuButtonMediator newMediator =
                new MenuButtonMediator(
                        mPropertyModel,
                        false,
                        () -> false,
                        mRequestRenderRunnable,
                        mThemeColorProvider,
                        () -> false,
                        mControlsVisibilityDelegate,
                        mFocusFunction,
                        mAppMenuSupplier,
                        mWindowAndroid,
                        () -> mMenuUiState.buttonState,
                        mOnMenuButtonClicked);

        doReturn(true).when(mActivity).isDestroyed();
        newMediator.updateStateChanged();

        assertFalse(mPropertyModel.get(MenuButtonProperties.SHOW_UPDATE_BADGE).mShowUpdateBadge);
        verify(mRequestRenderRunnable, never()).run();

        doReturn(false).when(mActivity).isDestroyed();
        newMediator.updateStateChanged();

        assertFalse(mPropertyModel.get(MenuButtonProperties.SHOW_UPDATE_BADGE).mShowUpdateBadge);
        verify(mRequestRenderRunnable, never()).run();

        mMenuUiState.buttonState = new MenuButtonState();
        newMediator.updateStateChanged();

        assertFalse(mPropertyModel.get(MenuButtonProperties.SHOW_UPDATE_BADGE).mShowUpdateBadge);
        verify(mRequestRenderRunnable, never()).run();
    }

    @Test
    public void testDestroyIsSafe() {
        mMenuButtonMediator.destroy();
        // It should be crash-safe to call public methods, but the results aren't meaningful.
        mMenuButtonMediator.getMenuButtonHelperSupplier();
        mMenuButtonMediator.onMenuHighlightChanged(true);
        mMenuButtonMediator.onMenuVisibilityChanged(false);
        mMenuButtonMediator.updateReloadingState(true);
        mMenuButtonMediator.updateStateChanged();
    }

    @Test
    public void testKeyboardIsDismissedWhenMenuShows() {
        doReturn(mUtilityView).when(mActivity).getCurrentFocus();
        mMenuButtonMediator.onMenuVisibilityChanged(true);
        verify(mKeyboardDelegate).hideKeyboard(eq(mUtilityView));
    }

    @Test
    public void testKeyboardIsNotDismissedWhenMenuShowsWithNoFocusedViews() {
        doReturn(null).when(mActivity).getCurrentFocus();
        mMenuButtonMediator.onMenuVisibilityChanged(true);
        verify(mKeyboardDelegate, never()).hideKeyboard(any());
    }
}
