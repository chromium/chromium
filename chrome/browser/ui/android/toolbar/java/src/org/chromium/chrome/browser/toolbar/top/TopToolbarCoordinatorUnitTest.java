// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.toolbar.back_button.BackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.forward_button.ForwardButtonCoordinator;
import org.chromium.chrome.browser.toolbar.home_button.HomeButtonCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup.HistoryDelegate;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionDelegate;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Unit tests for {@link TopToolbarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TopToolbarCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ToolbarControlContainer mControlContainer;
    @Mock private ToolbarTablet mToolbarLayout;
    @Mock private ToolbarDataProvider mToolbarDataProvider;
    @Mock private ToolbarTabController mTabController;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private OneshotSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    @Mock private ThemeColorProvider mNormalThemeColorProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;
    @Mock private MenuButtonCoordinator mBrowsingModeMenuButtonCoordinator;
    @Mock private ToggleTabStackButtonCoordinator mTabSwitcherButtonCoordinator;
    @Mock private Supplier<org.chromium.ui.resources.ResourceManager> mResourceManagerSupplier;
    @Mock private HistoryDelegate mHistoryDelegate;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private TabObscuringHandler mTabObscuringHandler;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private OneshotSupplier<TabStripTransitionDelegate> mTabStripTransitionDelegateSupplier;
    @Mock private TabStripTransitionHandler mTabStripTransitionHandler;
    @Mock private View.OnLongClickListener mOnLongClickListener;
    @Mock private ToolbarProgressBar mProgressBar;
    @Mock private BackButtonCoordinator mBackButtonCoordinator;
    @Mock private ForwardButtonCoordinator mForwardButtonCoordinator;
    @Mock private HomeButtonCoordinator mHomeButtonCoordinator;
    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private TopToolbarOverlayCoordinator mOverlayCoordinator;
    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    @Mock private OneshotSupplier<OmniboxStub> mOmniboxStubSupplier;
    @Mock private SigninAndHistorySyncActivityLauncher mSigninAndHistorySyncActivityLauncher;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ActivityResultTracker mActivityResultTracker;
    @Mock private DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Runnable mOnSigninTapped;
    @Mock private Profile mProfile;
    @Mock private View mLocationBarView;
    @Mock private Resources mResources;
    @Mock private CoordinatorLayout.LayoutParams mCoordinatorLayoutParams;

    private final MonotonicObservableSupplier<AppMenuButtonHelper> mAppMenuButtonHelperSupplier =
            ObservableSuppliers.createMonotonic();
    private final MonotonicObservableSupplier<Integer> mTabCountSupplier =
            ObservableSuppliers.createMonotonic();
    private final NonNullObservableSupplier<Boolean> mHomepageEnabledSupplier =
            ObservableSuppliers.alwaysTrue();
    private final NullableObservableSupplier<Integer> mConstraintsSupplier =
            ObservableSuppliers.createNullable();
    private final NonNullObservableSupplier<Boolean> mCompositorInMotionSupplier =
            ObservableSuppliers.alwaysFalse();
    private final BrowserStateBrowserControlsVisibilityDelegate
            mBrowserStateBrowserControlsVisibilityDelegate =
                    new BrowserStateBrowserControlsVisibilityDelegate(
                            ObservableSuppliers.alwaysFalse());
    private final NullableObservableSupplier<Tab> mTabSupplier =
            ObservableSuppliers.createNullable();
    private final NonNullObservableSupplier<Boolean> mToolbarNavControlsEnabledSupplier =
            ObservableSuppliers.alwaysTrue();
    private final MonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();

    private TopToolbarCoordinator mCoordinator;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        when(mToolbarLayout.getContext()).thenReturn(context);
        when(mToolbarLayout.getResources()).thenReturn(mResources);
        when(mToolbarLayout.findViewById(R.id.location_bar)).thenReturn(mLocationBarView);
        when(mToolbarLayout.indexOfChild(mLocationBarView)).thenReturn(0);
        when(mToolbarLayout.getToolbarDataProvider()).thenReturn(mToolbarDataProvider);
        when(mToolbarDataProvider.getProfile()).thenReturn(mProfile);
        when(mControlContainer.mutateToolbarLayoutParams()).thenReturn(mCoordinatorLayoutParams);

        List<ButtonDataProvider> buttonDataProviders = new ArrayList<>();

        mCoordinator =
                new TopToolbarCoordinator(
                        mControlContainer,
                        mToolbarLayout,
                        mToolbarDataProvider,
                        mTabController,
                        mUserEducationHelper,
                        buttonDataProviders,
                        mLayoutStateProviderSupplier,
                        mNormalThemeColorProvider,
                        mIncognitoStateProvider,
                        mBrowsingModeMenuButtonCoordinator,
                        mAppMenuButtonHelperSupplier,
                        mTabSwitcherButtonCoordinator,
                        mTabCountSupplier,
                        mHomepageEnabledSupplier,
                        mResourceManagerSupplier,
                        mHistoryDelegate,
                        /* initializeWithIncognitoColors= */ false,
                        mConstraintsSupplier,
                        mCompositorInMotionSupplier,
                        mBrowserStateBrowserControlsVisibilityDelegate,
                        mFullscreenManager,
                        mTabObscuringHandler,
                        mDesktopWindowStateManager,
                        mTabStripTransitionDelegateSupplier,
                        mTabStripTransitionHandler,
                        mOnLongClickListener,
                        mProgressBar,
                        mTabSupplier,
                        mToolbarNavControlsEnabledSupplier,
                        mBackButtonCoordinator,
                        mForwardButtonCoordinator,
                        mHomeButtonCoordinator,
                        mTopControlsStacker,
                        mBrowserControlsVisibilityManager,
                        /* incognitoWindowCountSupplier= */ () -> 0,
                        mProfileSupplier,
                        mOmniboxStubSupplier,
                        mSigninAndHistorySyncActivityLauncher,
                        mWindowAndroid,
                        mActivityResultTracker,
                        mDeviceLockActivityLauncher,
                        mBottomSheetController,
                        mModalDialogManager,
                        mSnackbarManager,
                        mOnSigninTapped,
                        /* suppressTabStripAtStart= */ false);
    }

    @Test
    public void testGlicActionChipVisibility_Toggled() {
        SettableNonNullObservableSupplier<Boolean> isVerticalTabActiveSupplier =
                ObservableSuppliers.createNonNull(false);
        SettableNonNullObservableSupplier<Boolean> isGlicPinnedSupplier =
                ObservableSuppliers.createNonNull(false);
        IncognitoStateProvider incognitoStateProvider = new IncognitoStateProvider();
        TabModelSelector tabModelSelector = Mockito.mock();
        MonotonicObservableSupplier<TabModel> currentTabModelSupplier =
                ObservableSuppliers.createMonotonic();
        when(tabModelSelector.getCurrentTabModelSupplier()).thenReturn(currentTabModelSupplier);
        incognitoStateProvider.setTabModelSelector(tabModelSelector);
        clearInvocations(mToolbarLayout);

        InOrder inOrder = Mockito.inOrder(mToolbarLayout);
        mCoordinator.observeGlicVerticalTabs(
                isVerticalTabActiveSupplier, isGlicPinnedSupplier, incognitoStateProvider);

        // 1. Initial state (both false) -> Glic chip hidden.
        inOrder.verify(mToolbarLayout).setGlicActionChipVisibility(eq(false), any());

        // 2. VT active = true, Glic pinned = false -> Glic chip hidden.
        isVerticalTabActiveSupplier.set(true);
        inOrder.verify(mToolbarLayout).setGlicActionChipVisibility(eq(false), any());

        // 3. VT active = true, Glic pinned = true -> Glic chip visible!
        isGlicPinnedSupplier.set(true);
        inOrder.verify(mToolbarLayout).setGlicActionChipVisibility(eq(true), any());

        // 4. VT active = false, Glic pinned = true -> Glic chip hidden.
        isVerticalTabActiveSupplier.set(false);
        inOrder.verify(mToolbarLayout).setGlicActionChipVisibility(eq(false), any());

        // 5. Repeat the combinations above with Incognito = true. Glic should remain hidden.
        when(tabModelSelector.isIncognitoSelected()).thenReturn(true);

        incognitoStateProvider.setIncognitoStateForTesting(true);
        inOrder.verify(mToolbarLayout).setGlicActionChipVisibility(eq(false), any());

        isVerticalTabActiveSupplier.set(true);
        inOrder.verify(mToolbarLayout).setGlicActionChipVisibility(eq(false), any());

        isGlicPinnedSupplier.set(false);
        inOrder.verify(mToolbarLayout).setGlicActionChipVisibility(eq(false), any());

        isVerticalTabActiveSupplier.set(false);
        inOrder.verify(mToolbarLayout).setGlicActionChipVisibility(eq(false), any());
    }

    @Test
    public void testGlicVerticalTabsObserver_destroy() {
        SettableNonNullObservableSupplier<Boolean> isVerticalTabActiveSupplier =
                ObservableSuppliers.createNonNull(false);
        SettableNonNullObservableSupplier<Boolean> isGlicPinnedSupplier =
                ObservableSuppliers.createNonNull(false);
        IncognitoStateProvider incognitoStateProvider = new IncognitoStateProvider();
        mCoordinator.observeGlicVerticalTabs(
                isVerticalTabActiveSupplier, isGlicPinnedSupplier, incognitoStateProvider);
        assertEquals(1, isVerticalTabActiveSupplier.getObserverCount());
        assertEquals(1, isGlicPinnedSupplier.getObserverCount());
        assertEquals(1, incognitoStateProvider.getObserverCountForTesting());

        mCoordinator.destroy();
        assertEquals(0, isVerticalTabActiveSupplier.getObserverCount());
        assertEquals(0, isGlicPinnedSupplier.getObserverCount());
        assertEquals(0, incognitoStateProvider.getObserverCountForTesting());
    }

    @Test
    public void testOnToolbarHairlineSuppressedChanged() {
        mCoordinator.setOverlayCoordinatorForTesting(mOverlayCoordinator);
        mCoordinator.onToolbarHairlineSuppressedChanged(true);
        verify(mToolbarLayout).onToolbarHairlineSuppressedChanged(true);
        verify(mOverlayCoordinator).onToolbarHairlineSuppressedChanged(true);
    }
}
