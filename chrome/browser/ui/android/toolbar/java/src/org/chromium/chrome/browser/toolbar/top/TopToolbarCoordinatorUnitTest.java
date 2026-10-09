// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
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

    @Mock private ResourceFactory.Natives mResourceFactoryJni;
    @Mock private LocationBarCoordinator mLocationBarCoordinator;
    @Mock private ToolbarDataProvider mToolbarDataProvider;
    @Mock private ToolbarTabController mTabController;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private OneshotSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    @Mock private ThemeColorProvider mNormalThemeColorProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;
    @Mock private TabModelSelector mTabModelSelector;
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
    @Mock private View.OnLongClickListener mGlicLongClickListener;

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
    private final NonNullObservableSupplier<Boolean> mDownloadButtonShouldShowSupplier =
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

    private Activity mActivity;
    private ToolbarControlContainer mControlContainer;
    private ToolbarTablet mToolbarLayout;
    private ToolbarProgressBar mProgressBar;
    private TopToolbarCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        ResourceFactoryJni.setInstanceForTesting(mResourceFactoryJni);

        mControlContainer =
                (ToolbarControlContainer)
                        mActivity.getLayoutInflater().inflate(R.layout.control_container, null);
        mControlContainer.initWithToolbar(
                R.layout.toolbar_tablet, R.dimen.toolbar_height_no_shadow);
        mControlContainer.findViewById(R.id.toolbar_hairline).layout(0, 0, 100, 3);
        mToolbarLayout = mControlContainer.findViewById(R.id.toolbar);
        mToolbarLayout.layout(0, 0, 1000, 100);
        mProgressBar = new ToolbarProgressBar(mActivity, /* attrs= */ null);

        when(mLocationBarCoordinator.getFuseboxStateSupplier())
                .thenReturn(ObservableSuppliers.createNonNull(FuseboxState.DISABLED));
        mToolbarLayout.setLocationBarCoordinator(mLocationBarCoordinator);

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
                        mDownloadButtonShouldShowSupplier,
                        /* suppressTabStripAtStart= */ false);
    }

    private boolean isGlicActionChipVisible() {
        View chip = mToolbarLayout.getGlicActionChipView();
        return chip != null && chip.getVisibility() == View.VISIBLE;
    }

    private void simulateCapture(int captureHeight, int topOffsetInCapture) {
        mToolbarLayout.layout(0, topOffsetInCapture, 1000, topOffsetInCapture + 100);
        if (captureHeight > 0) {
            when(mToolbarDataProvider.getUrlBarData()).thenReturn(UrlBarData.EMPTY);
            when(mLocationBarCoordinator.getContainerView())
                    .thenReturn(mToolbarLayout.findViewById(R.id.location_bar));
            mControlContainer.findViewById(R.id.toolbar_container).layout(0, 0, 100, captureHeight);
            mControlContainer.getToolbarResourceAdapter().triggerBitmapCapture();
        }
        assertEquals(captureHeight, mControlContainer.getToolbarCaptureHeight());
        assertEquals(topOffsetInCapture, mControlContainer.getToolbarTopOffsetInCapture());
    }

    @Test
    public void testGlicActionChipVisibility_Toggled() {
        SettableNonNullObservableSupplier<Boolean> isVerticalTabActiveSupplier =
                ObservableSuppliers.createNonNull(false);
        SettableNonNullObservableSupplier<Boolean> isGlicPinnedSupplier =
                ObservableSuppliers.createNonNull(false);
        IncognitoStateProvider incognitoStateProvider = new IncognitoStateProvider();
        MonotonicObservableSupplier<TabModel> currentTabModelSupplier =
                ObservableSuppliers.createMonotonic();
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(currentTabModelSupplier);
        incognitoStateProvider.setTabModelSelector(mTabModelSelector);

        mCoordinator.observeGlicVerticalTabs(
                isVerticalTabActiveSupplier,
                isGlicPinnedSupplier,
                incognitoStateProvider,
                mGlicLongClickListener);

        // 1. Initial state (both false) -> Glic chip hidden.
        assertFalse(isGlicActionChipVisible());

        // 2. VT active = true, Glic pinned = false -> Glic chip hidden.
        isVerticalTabActiveSupplier.set(true);
        assertFalse(isGlicActionChipVisible());

        // 3. VT active = true, Glic pinned = true -> Glic chip visible.
        isGlicPinnedSupplier.set(true);
        assertTrue(isGlicActionChipVisible());

        // 4. VT active = false, Glic pinned = true -> Glic chip hidden.
        isVerticalTabActiveSupplier.set(false);
        assertFalse(isGlicActionChipVisible());

        // In Incognito mode, button visibility should still reflect VT active and pinned state.
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);

        // VT active = false, pinned = true -> Glic chip hidden.
        incognitoStateProvider.setIncognitoStateForTesting(true);
        assertFalse(isGlicActionChipVisible());

        // VT active = true, pinned = true -> Glic chip visible.
        isVerticalTabActiveSupplier.set(true);
        assertTrue(isGlicActionChipVisible());

        // VT active = true, pinned = false -> Glic chip hidden.
        isGlicPinnedSupplier.set(false);
        assertFalse(isGlicActionChipVisible());

        // VT active = false, pinned = false -> Glic chip hidden.
        isVerticalTabActiveSupplier.set(false);
        assertFalse(isGlicActionChipVisible());
    }

    @Test
    public void testGlicVerticalTabsObserver_destroy() {
        SettableNonNullObservableSupplier<Boolean> isVerticalTabActiveSupplier =
                ObservableSuppliers.createNonNull(false);
        SettableNonNullObservableSupplier<Boolean> isGlicPinnedSupplier =
                ObservableSuppliers.createNonNull(false);
        IncognitoStateProvider incognitoStateProvider = new IncognitoStateProvider();
        mCoordinator.observeGlicVerticalTabs(
                isVerticalTabActiveSupplier,
                isGlicPinnedSupplier,
                incognitoStateProvider,
                mGlicLongClickListener);
        assertEquals(1, isVerticalTabActiveSupplier.getObserverCount());
        assertEquals(1, isGlicPinnedSupplier.getObserverCount());
        assertEquals(1, incognitoStateProvider.getObserverCountForTesting());

        mCoordinator.destroy();
        assertEquals(0, isVerticalTabActiveSupplier.getObserverCount());
        assertEquals(0, isGlicPinnedSupplier.getObserverCount());
        assertEquals(0, incognitoStateProvider.getObserverCountForTesting());
    }

    @Test
    public void testHiddenControlsKeepHairlineCaptureOffscreen() {
        // Reproduces the installed web app cold launch: browser controls are fully
        // hidden with browser-applied offsets, the browser visibility delegate stays
        // at BOTH, and the resting content offset sits exactly at the zero min-height
        // boundary. The capture is toolbar + hairline tall, so parking the layer at
        // -toolbarHeight leaves the hairline row visible at the top of the screen.
        mCoordinator.setOverlayCoordinatorForTesting(mOverlayCoordinator);
        when(mBrowserControlsVisibilityManager.getBrowserVisibilityDelegate())
                .thenReturn(mBrowserStateBrowserControlsVisibilityDelegate);
        assertEquals(
                BrowserControlsState.BOTH,
                (int) mBrowserStateBrowserControlsVisibilityDelegate.get());
        when(mBrowserControlsVisibilityManager.getTopControlsMinHeight()).thenReturn(0);
        when(mBrowserControlsVisibilityManager.getTopControlsHairlineHeight()).thenReturn(3);
        when(mBrowserControlsVisibilityManager.getContentOffset()).thenReturn(0);
        when(mBrowserControlsVisibilityManager.getBrowserControlHiddenRatio()).thenReturn(1f);
        simulateCapture(/* captureHeight= */ 150, /* topOffsetInCapture= */ 0);

        mCoordinator.onBrowserControlsOffsetUpdate(-147, /* reachRestingPosition= */ true);

        verify(mOverlayCoordinator).setYOffset(-150);
    }

    @Test
    public void testVisibleControlsAtMinHeightBoundaryKeepYOffset() {
        // Fully visible controls resting at the zero min-height boundary must not be
        // shifted; the hairline is on-screen by design (crbug.com/512898018).
        mCoordinator.setOverlayCoordinatorForTesting(mOverlayCoordinator);
        when(mBrowserControlsVisibilityManager.getBrowserVisibilityDelegate())
                .thenReturn(mBrowserStateBrowserControlsVisibilityDelegate);
        when(mBrowserControlsVisibilityManager.getTopControlsMinHeight()).thenReturn(0);
        when(mBrowserControlsVisibilityManager.getTopControlsHairlineHeight()).thenReturn(3);
        when(mBrowserControlsVisibilityManager.getContentOffset()).thenReturn(0);
        when(mBrowserControlsVisibilityManager.getBrowserControlHiddenRatio()).thenReturn(0f);
        simulateCapture(/* captureHeight= */ 150, /* topOffsetInCapture= */ 0);

        mCoordinator.onBrowserControlsOffsetUpdate(0, /* reachRestingPosition= */ true);

        verify(mOverlayCoordinator).setYOffset(0);
    }

    @Test
    public void testSceneLayerYOffset_ShiftedUpByToolbarOffsetInCapture() {
        // The capture starts at the tab strip, so the toolbar sits 40px down into the bitmap. The
        // scene layer has to be shifted up by that much for the toolbar itself to land on the
        // offset the browser controls asked for.
        mCoordinator.setOverlayCoordinatorForTesting(mOverlayCoordinator);
        setUpVisibleRestingControls();

        simulateCapture(/* captureHeight= */ 190, /* topOffsetInCapture= */ 40);

        // Fully visible: finalYOffset = 40 - 40 = 0.
        mCoordinator.onBrowserControlsOffsetUpdate(40, /* reachRestingPosition= */ false);

        verify(mOverlayCoordinator).setYOffset(0);

        // Partially scrolled off: finalYOffset = 20 - 40 = -20.
        mCoordinator.onBrowserControlsOffsetUpdate(20, /* reachRestingPosition= */ false);

        verify(mOverlayCoordinator).setYOffset(-20);
    }

    @Test
    public void testSceneLayerYOffset_NothingAboveToolbarInCapture() {
        // Nothing in the capture sits above the toolbar, either because there is no tab strip
        // (phones), or because there is no capture yet, in which case there is no bitmap to
        // position and the offset reads 0. The scene layer is not shifted either way.
        mCoordinator.setOverlayCoordinatorForTesting(mOverlayCoordinator);
        setUpVisibleRestingControls();

        for (int captureHeight : new int[] {0, 150}) {
            clearInvocations(mOverlayCoordinator);
            simulateCapture(captureHeight, /* topOffsetInCapture= */ 0);

            // finalYOffset = 40 - 0 = 40.
            mCoordinator.onBrowserControlsOffsetUpdate(40, /* reachRestingPosition= */ false);

            verify(mOverlayCoordinator).setYOffset(40);
        }
    }

    @Test
    public void testSceneLayerYOffset_CaptureHeightIsIrrelevant() {
        // The offset is measured against the view that is rasterized rather than derived from
        // separately measured view heights, so the capture's height never enters the computation:
        // neither dp -> px rounding jitter (189/191), nor extra height below the toolbar such as
        // an open fusebox (240), nor a stale bitmap whose container has since grown.
        mCoordinator.setOverlayCoordinatorForTesting(mOverlayCoordinator);
        setUpVisibleRestingControls();

        for (int captureHeight : new int[] {189, 190, 191, 240}) {
            clearInvocations(mOverlayCoordinator);
            simulateCapture(captureHeight, /* topOffsetInCapture= */ 40);

            mCoordinator.onBrowserControlsOffsetUpdate(40, /* reachRestingPosition= */ false);

            verify(mOverlayCoordinator).setYOffset(0);
        }
    }

    /**
     * Stubs browser controls that are fully visible and at rest, which is the state in which no
     * hairline adjustment is applied, leaving the scene layer offset to be purely capture math.
     */
    private void setUpVisibleRestingControls() {
        when(mBrowserControlsVisibilityManager.getBrowserVisibilityDelegate())
                .thenReturn(mBrowserStateBrowserControlsVisibilityDelegate);
        when(mBrowserControlsVisibilityManager.getTopControlsMinHeight()).thenReturn(0);
        when(mBrowserControlsVisibilityManager.getTopControlsHairlineHeight()).thenReturn(3);
        when(mBrowserControlsVisibilityManager.getContentOffset()).thenReturn(0);
        when(mBrowserControlsVisibilityManager.getBrowserControlHiddenRatio()).thenReturn(0f);
    }

    @Test
    public void testOnToolbarHairlineSuppressedChanged() {
        mCoordinator.setOverlayCoordinatorForTesting(mOverlayCoordinator);
        mCoordinator.onToolbarHairlineSuppressedChanged(true);
        assertTrue(mToolbarLayout.isToolbarHairlineSuppressed());
        verify(mOverlayCoordinator).onToolbarHairlineSuppressedChanged(true);

        mCoordinator.onToolbarHairlineSuppressedChanged(false);
        assertFalse(mToolbarLayout.isToolbarHairlineSuppressed());
        verify(mOverlayCoordinator).onToolbarHairlineSuppressedChanged(false);
    }

    @Test
    public void testShouldShowGlicToolbarButton_AndGetGlicActionChipView() {
        SettableNonNullObservableSupplier<Boolean> isVerticalTabActiveSupplier =
                ObservableSuppliers.createNonNull(false);
        SettableNonNullObservableSupplier<Boolean> isGlicPinnedSupplier =
                ObservableSuppliers.createNonNull(false);
        IncognitoStateProvider incognitoStateProvider = new IncognitoStateProvider();
        MonotonicObservableSupplier<TabModel> currentTabModelSupplier =
                ObservableSuppliers.createMonotonic();
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(currentTabModelSupplier);
        incognitoStateProvider.setTabModelSelector(mTabModelSelector);

        mCoordinator.observeGlicVerticalTabs(
                isVerticalTabActiveSupplier,
                isGlicPinnedSupplier,
                incognitoStateProvider,
                mGlicLongClickListener);

        assertFalse(mCoordinator.shouldShowGlicToolbarButton());

        // VerticalTabs active = true, Glic pinned = false -> false.
        isVerticalTabActiveSupplier.set(true);
        assertFalse(mCoordinator.shouldShowGlicToolbarButton());

        // VerticalTabs active = true, Glic pinned = true -> true.
        isGlicPinnedSupplier.set(true);
        assertTrue(mCoordinator.shouldShowGlicToolbarButton());

        View glicChip = mCoordinator.getGlicActionChipView();
        assertNotNull(glicChip);
        assertEquals(mToolbarLayout.findViewById(R.id.glic_action_chip), glicChip);

        // Verify long-click listener was passed to mToolbarLayout and forwards correctly when
        // triggered.
        glicChip.performLongClick();
        verify(mGlicLongClickListener).onLongClick(glicChip);

        // In incognito mode, button should still show if VT is active and Glic is pinned.
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);
        incognitoStateProvider.setIncognitoStateForTesting(true);
        assertTrue(mCoordinator.shouldShowGlicToolbarButton());
    }

    @Test
    public void testSetGlicPanelIsOpen() {
        mToolbarLayout.setGlicActionChipVisibility(
                /* visible= */ true, v -> {}, mGlicLongClickListener);
        View glicChip = mCoordinator.getGlicActionChipView();
        assertNotNull(glicChip);

        mCoordinator.setGlicPanelIsOpen(true);
        assertEquals(
                mActivity.getString(R.string.glic_tab_strip_button_tooltip_close),
                glicChip.getContentDescription());

        mCoordinator.setGlicPanelIsOpen(false);
        assertEquals(
                mActivity.getString(R.string.glic_tab_strip_button_tooltip),
                glicChip.getContentDescription());
    }

    @Test
    public void testGetTopControlVisibility() {
        when(mBrowserControlsVisibilityManager.getControlsPosition())
                .thenReturn(ControlsPosition.TOP);
        assertEquals(
                TopControlsStacker.TopControlVisibility.VISIBLE,
                mCoordinator.getTopControlVisibility());

        when(mBrowserControlsVisibilityManager.getControlsPosition())
                .thenReturn(ControlsPosition.BOTTOM);
        assertEquals(
                TopControlsStacker.TopControlVisibility.HIDDEN,
                mCoordinator.getTopControlVisibility());
    }

    @Test
    public void testOnLongClickListener() {
        assertNull(Shadows.shadowOf(mToolbarLayout).getOnLongClickListener());

        ToolbarControlContainer phoneControlContainer =
                (ToolbarControlContainer)
                        mActivity.getLayoutInflater().inflate(R.layout.control_container, null);
        phoneControlContainer.initWithToolbar(
                R.layout.toolbar_phone, R.dimen.toolbar_height_no_shadow);
        ToolbarPhone toolbarPhone = phoneControlContainer.findViewById(R.id.toolbar);

        new TopToolbarCoordinator(
                phoneControlContainer,
                toolbarPhone,
                mToolbarDataProvider,
                mTabController,
                mUserEducationHelper,
                new ArrayList<>(),
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
                mDownloadButtonShouldShowSupplier,
                /* suppressTabStripAtStart= */ false);
        assertEquals(mOnLongClickListener, Shadows.shadowOf(toolbarPhone).getOnLongClickListener());
    }
}
