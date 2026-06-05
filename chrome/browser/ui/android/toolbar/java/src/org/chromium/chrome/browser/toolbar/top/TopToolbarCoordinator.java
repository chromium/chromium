// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.build.NullUtil.assertNonNull;

import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewStub;
import android.widget.ImageButton;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.browser_controls.TopControlLayer;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ToolbarThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.back_button.BackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionsToolbarCoordinator;
import org.chromium.chrome.browser.toolbar.forward_button.ForwardButtonCoordinator;
import org.chromium.chrome.browser.toolbar.home_button.HomeButtonCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.reload_button.ReloadButtonCoordinator;
import org.chromium.chrome.browser.toolbar.signin_button.SigninButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup.HistoryDelegate;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionDelegate;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar.DrawingInfo;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.util.TokenHolder;

import java.util.List;
import java.util.function.Supplier;

/** A coordinator for the top toolbar component. */
@NullMarked
public class TopToolbarCoordinator implements Toolbar, TopControlLayer {
    private static final int UNSPECIFIED_TOOLBAR_OFFSET = -1234;

    /** Observes toolbar color or expanding state change. */
    public interface ToolbarColorObserver {
        /**
         * @param color The toolbar color.
         */
        void onToolbarColorChanged(@ColorInt int color);

        /**
         * Notifies the observer when the Toolbar is expanding or has collapsed.
         *
         * @param isToolbarExpanding Whether the toolbar is expanding.
         */
        void onToolbarExpandingOnNtp(boolean isToolbarExpanding);
    }

    private final ToolbarLayout mToolbarLayout;
    private final View mLocationBarView;
    private final SettableMonotonicObservableSupplier<Tracker> mTrackerSupplier;

    private OptionalBrowsingModeButtonController mOptionalButtonController;

    private @Nullable SigninButtonCoordinator mSigninButtonCoordinator;

    private final MenuButtonCoordinator mMenuButtonCoordinator;
    private @Nullable ReloadButtonCoordinator mReloadButtonCoordinator;
    private @Nullable final BackButtonCoordinator mBackButtonCoordinator;
    private MonotonicObservableSupplier<AppMenuButtonHelper> mAppMenuButtonHelperSupplier;

    /** Null until {@link #initializeWithNative} is called. */
    private @Nullable TabStripTransitionCoordinator mTabStripTransitionCoordinator;

    private ToolbarControlContainer mControlContainer;
    private final Supplier<ResourceManager> mResourceManagerSupplier;
    private @Nullable TopToolbarOverlayCoordinator mOverlayCoordinator;

    /**
     * The observer manager will receive all types of toolbar color change updates from toolbar
     * components and send the rendering toolbar color to the ToolbarColorObserver.
     */
    private final ToolbarColorObserverManager mToolbarColorObserverManager;

    private final TabObscuringHandler mTabObscuringHandler;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private final OneshotSupplier<TabStripTransitionDelegate> mTabStripTransitionDelegateSupplier;
    private final SettableNonNullObservableSupplier<Boolean> mNtpLoadingSupplier;
    private final BrowserControlsVisibilityManager mBrowserControls;
    private final TopControlsStacker mTopControlsStacker;

    private MonotonicObservableSupplier<Integer> mTabCountSupplier;

    /** Token used to block the tab strip transition when find in page toolbar is showing. */
    private int mFindToolbarToken = TokenHolder.INVALID_TOKEN;

    private final int mIndexOfLocationBarInToolbar;
    private int mLayerYOffset = UNSPECIFIED_TOOLBAR_OFFSET;
    private boolean mIsHairlineVisible = true;

    /**
     * Creates a new {@link TopToolbarCoordinator}.
     *
     * @param controlContainer The {@link ToolbarControlContainer} for the containing activity.
     * @param toolbarLayout The {@link ToolbarLayout}.
     * @param toolbarDataProvider The provider for toolbar data.
     * @param tabController The controller that handles interactions with the tab.
     * @param userEducationHelper Helper class for showing in-product help text bubbles.
     * @param buttonDataProviders List of classes that wish to display an optional button in the
     *     browsing mode toolbar.
     * @param layoutStateProviderSupplier Supplier of the {@link LayoutStateProvider}.
     * @param normalThemeColorProvider The {@link ThemeColorProvider} for normal mode.
     * @param incognitoStateProvider The {@link IncognitoStateProvider} for observering incognito
     *     state.
     * @param browsingModeMenuButtonCoordinator Root component for app menu.
     * @param appMenuButtonHelperSupplier For specific handling of the app menu button.
     * @param tabCountSupplier Supplier of {@link
     *     org.chromium.chrome.browser.toolbar.CustomTabCount}.
     * @param homepageEnabledSupplier Supplier of whether Home button is enabled.
     * @param resourceManagerSupplier A supplier of a resource manager for native textures.
     * @param historyDelegate Delegate used to display navigation history.
     * @param initializeWithIncognitoColors Whether the toolbar should be initialized with incognito
     *     colors.
     * @param constraintsSupplier Supplier for browser controls constraints.
     * @param compositorInMotionSupplier Whether there is an ongoing touch or gesture.
     * @param browserStateBrowserControlsVisibilityDelegate Used to keep controls locked when
     *     captures are stale and not able to be taken.
     * @param fullscreenManager Used to check whether in fullscreen.
     * @param tabObscuringHandler Delegate object handling obscuring views.
     * @param desktopWindowStateManager The {@link DesktopWindowStateManager} instance.
     * @param tabStripTransitionDelegateSupplier Supplier for the {@link
     *     TabStripTransitionDelegate}.
     * @param tabStripTransitionHandler TabStripTransitionHandler instance.
     * @param onLongClickListener OnLongClickListener for the toolbar.
     * @param homeButtonCoordinator The {@link HomeButtonCoordinator} to manage the display and
     *     behavior of the home button.
     * @param topControlsStacker The TopControlsStacker for child objects to check state from.
     * @param browserControlsVisibilityManager BrowserControlsStateProvider instance.
     */
    public TopToolbarCoordinator(
            ToolbarControlContainer controlContainer,
            ToolbarLayout toolbarLayout,
            ToolbarDataProvider toolbarDataProvider,
            ToolbarTabController tabController,
            UserEducationHelper userEducationHelper,
            List<ButtonDataProvider> buttonDataProviders,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            ThemeColorProvider normalThemeColorProvider,
            IncognitoStateProvider incognitoStateProvider,
            MenuButtonCoordinator browsingModeMenuButtonCoordinator,
            MonotonicObservableSupplier<AppMenuButtonHelper> appMenuButtonHelperSupplier,
            @Nullable ToggleTabStackButtonCoordinator tabSwitcherButtonCoordinator,
            MonotonicObservableSupplier<Integer> tabCountSupplier,
            NonNullObservableSupplier<Boolean> homepageEnabledSupplier,
            Supplier<ResourceManager> resourceManagerSupplier,
            HistoryDelegate historyDelegate,
            boolean initializeWithIncognitoColors,
            NullableObservableSupplier<@BrowserControlsState Integer> constraintsSupplier,
            NonNullObservableSupplier<Boolean> compositorInMotionSupplier,
            BrowserStateBrowserControlsVisibilityDelegate
                    browserStateBrowserControlsVisibilityDelegate,
            FullscreenManager fullscreenManager,
            TabObscuringHandler tabObscuringHandler,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            OneshotSupplier<TabStripTransitionDelegate> tabStripTransitionDelegateSupplier,
            TabStripTransitionHandler tabStripTransitionHandler,
            @Nullable OnLongClickListener onLongClickListener,
            ToolbarProgressBar progressBar,
            NullableObservableSupplier<Tab> tabSupplier,
            NonNullObservableSupplier<Boolean> toolbarNavControlsEnabledSupplier,
            @Nullable BackButtonCoordinator backButtonCoordinator,
            @Nullable ForwardButtonCoordinator forwardButtonCoordinator,
            HomeButtonCoordinator homeButtonCoordinator,
            TopControlsStacker topControlsStacker,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            Supplier<Integer> incognitoWindowCountSupplier,
            MonotonicObservableSupplier<Profile> profileSupplier,
            OneshotSupplier<OmniboxStub> omniboxStubSupplier,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher,
            WindowAndroid windowAndroid,
            ActivityResultTracker activityResultTracker,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            SnackbarManager snackbarManager) {
        mToolbarLayout = toolbarLayout;
        mMenuButtonCoordinator = browsingModeMenuButtonCoordinator;
        mControlContainer = controlContainer;
        mBackButtonCoordinator = backButtonCoordinator;
        mOptionalButtonController =
                new OptionalBrowsingModeButtonController(
                        buttonDataProviders,
                        userEducationHelper,
                        mToolbarLayout,
                        () -> toolbarDataProvider.getTab());

        if (SigninFeatureMap.sSigninLevelUpButton.isEnabled()) {
            ViewStub signinButtonStub = mToolbarLayout.findViewById(R.id.signin_button_stub);
            if (signinButtonStub != null) {
                mSigninButtonCoordinator =
                        new SigninButtonCoordinator(
                                toolbarLayout.getContext(),
                                windowAndroid,
                                signinButtonStub,
                                tabSupplier,
                                omniboxStubSupplier,
                                mToolbarLayout::beginButtonTransition,
                                profileSupplier,
                                signinAndHistorySyncActivityLauncher,
                                activityResultTracker,
                                deviceLockActivityLauncher,
                                bottomSheetController,
                                modalDialogManager,
                                snackbarManager,
                                normalThemeColorProvider,
                                incognitoStateProvider);
            }
        }
        mResourceManagerSupplier = resourceManagerSupplier;
        mTabCountSupplier = tabCountSupplier;
        mToolbarColorObserverManager = new ToolbarColorObserverManager();
        mToolbarLayout.setToolbarColorObserver(mToolbarColorObserverManager);
        mTabObscuringHandler = tabObscuringHandler;
        mDesktopWindowStateManager = desktopWindowStateManager;
        mTrackerSupplier = ObservableSuppliers.createMonotonic();
        mNtpLoadingSupplier = ObservableSuppliers.createNonNull(false);
        mTabStripTransitionDelegateSupplier = tabStripTransitionDelegateSupplier;
        mToolbarLayout.setOnLongClickListener(onLongClickListener);
        mLocationBarView = mToolbarLayout.findViewById(R.id.location_bar);
        mIndexOfLocationBarInToolbar = mToolbarLayout.indexOfChild(mLocationBarView);
        mBrowserControls = browserControlsVisibilityManager;
        mTopControlsStacker = topControlsStacker;

        ImageButton reloadButton = mControlContainer.findViewById(R.id.refresh_button);
        if (reloadButton != null) {
            mReloadButtonCoordinator =
                    new ReloadButtonCoordinator(
                            reloadButton,
                            ignoreCache -> {
                                var omniboxStub = getLocationBar().getOmniboxStub();
                                if (omniboxStub != null) {
                                    omniboxStub.endInput();
                                }
                                tabController.stopOrReloadCurrentTab(ignoreCache);
                            },
                            tabSupplier,
                            mNtpLoadingSupplier,
                            toolbarNavControlsEnabledSupplier,
                            normalThemeColorProvider,
                            incognitoStateProvider,
                            /* isWebApp= */ false);
        }

        controlContainer.setPostInitializationDependencies(
                this,
                toolbarLayout,
                initializeWithIncognitoColors,
                constraintsSupplier,
                toolbarDataProvider::getTab,
                compositorInMotionSupplier,
                browserStateBrowserControlsVisibilityDelegate,
                layoutStateProviderSupplier,
                fullscreenManager,
                toolbarDataProvider,
                browserControlsVisibilityManager);
        mToolbarLayout.initialize(
                toolbarDataProvider,
                tabController,
                mMenuButtonCoordinator,
                tabSwitcherButtonCoordinator,
                historyDelegate,
                userEducationHelper,
                mTrackerSupplier,
                progressBar,
                mReloadButtonCoordinator,
                mBackButtonCoordinator,
                forwardButtonCoordinator,
                homeButtonCoordinator,
                mSigninButtonCoordinator,
                normalThemeColorProvider,
                incognitoStateProvider,
                incognitoWindowCountSupplier,
                windowAndroid);
        mAppMenuButtonHelperSupplier = appMenuButtonHelperSupplier;
        new OneShotCallback<>(mAppMenuButtonHelperSupplier, this::setAppMenuButtonHelper);
        homepageEnabledSupplier.addSyncObserverAndCallIfNonNull(
                (show) -> mToolbarLayout.onHomeButtonIsEnabledUpdate(show));

        // When we can force height adjustment on start up, we need to create tab strip transition
        // earlier, before native is ready.
        // TODO(crbug.com/450970998): Once launched, it's safe to always call
        // maybeInitializeTabStripTransitionCoordinator on all form factors.
        if (BrowserControlsUtils.isForceTopChromeHeightAdjustmentOnStartupEnabled(
                toolbarLayout.getContext())) {
            mTabStripTransitionCoordinator =
                    maybeInitializeTabStripTransitionCoordinator(
                            mToolbarLayout,
                            mControlContainer,
                            mTabObscuringHandler,
                            mDesktopWindowStateManager,
                            mTabStripTransitionDelegateSupplier,
                            tabStripTransitionHandler);
        }

        // Add the layer after toolbar / control container is initialized.
        mTopControlsStacker.addControl(this);
        mTopControlsStacker.requestLayerUpdatePost(false);

        if (ChromeFeatureList.sToolbarSnapshotRefactor.isEnabled()) {
            // Remove the top margin directly from the toolbar.
            mControlContainer.mutateToolbarLayoutParams().topMargin = 0;
        }
    }

    /**
     * @param appMenuButtonHelper The helper for managing menu button interactions.
     */
    public void setAppMenuButtonHelper(AppMenuButtonHelper appMenuButtonHelper) {
        mToolbarLayout.setAppMenuButtonHelper(appMenuButtonHelper);
    }

    /**
     * Initialize the coordinator with the components that have native initialization dependencies.
     *
     * <p>Calling this must occur after the native library have completely loaded.
     *
     * @param profile The primary Profile associated with this Toolbar.
     * @param layoutUpdater A {@link Runnable} used to request layout update upon scene change.
     * @param bookmarkClickHandler The click handler for the bookmarks button.
     * @param customTabsBackClickHandler The click handler for the custom tabs back button.
     * @param layoutManager A {@link LayoutManager} used to watch for scene changes.
     * @param tabSupplier Supplier of the activity tab.
     * @param browserControlsVisibilityManager {@link BrowserControlsVisibilityManager} to access
     *     browser controls offsets and visibility.
     * @param toolbarThemeColorProvider {@link ThemeColorProvider} for top UI.
     * @param bottomToolbarControlsOffsetSupplier Supplier of the offset, relative to the bottom of
     *     the viewport, of the bottom-anchored toolbar.
     * @param suppressToolbarSceneLayerSupplier Supplier for whether suppress the update to the
     *     toolbar scene layer.
     * @param progressInfoCallback Callback when progress bar DrawingInfo has an update.
     * @param captureResourceIdSupplier Provides an id for the captured resource shown by the
     *     compositor.
     * @param tabStripTransitionHandler Handler that response to tab strip transition.
     */
    public void initializeWithNative(
            Profile profile,
            Runnable layoutUpdater,
            @Nullable OnClickListener bookmarkClickHandler,
            @Nullable OnClickListener customTabsBackClickHandler,
            LayoutManager layoutManager,
            NullableObservableSupplier<Tab> tabSupplier,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            ToolbarThemeColorProvider toolbarThemeColorProvider,
            NonNullObservableSupplier<Integer> bottomToolbarControlsOffsetSupplier,
            NonNullObservableSupplier<Boolean> suppressToolbarSceneLayerSupplier,
            Callback<DrawingInfo> progressInfoCallback,
            MonotonicObservableSupplier<Long> captureResourceIdSupplier,
            TabStripTransitionHandler tabStripTransitionHandler) {
        mTrackerSupplier.set(TrackerFactory.getTrackerForProfile(profile));
        mToolbarLayout.setTabCountSupplier(mTabCountSupplier);
        getLocationBar().updateVisualsForState();
        mToolbarLayout.setBookmarkClickHandler(bookmarkClickHandler);
        mToolbarLayout.setCustomTabCloseClickHandler(customTabsBackClickHandler);
        mToolbarLayout.setLayoutUpdater(layoutUpdater);

        mToolbarLayout.onNativeLibraryReady();

        // If fullscreen is disabled, don't bother creating this overlay; only the android view will
        // ever be shown.
        // TODO: Without the overlay, the toolbar will somehow have a 1 pixel transparent border
        // which will become a visible artifact when the web contents background has a big
        // difference with the toolbar background color defined by system color theme. So we still
        // enable the overlay on XR devices. See https://crbug.com/377982076.
        if (DeviceClassManager.enableFullscreen() || DeviceInfo.isXr()) {
            int layoutsToShowOn = LayoutType.BROWSING | LayoutType.TAB_SWITCHER;
            mOverlayCoordinator =
                    new TopToolbarOverlayCoordinator(
                            mToolbarLayout.getContext(),
                            layoutManager,
                            progressInfoCallback,
                            tabSupplier,
                            browserControlsVisibilityManager,
                            mResourceManagerSupplier,
                            toolbarThemeColorProvider,
                            bottomToolbarControlsOffsetSupplier,
                            suppressToolbarSceneLayerSupplier,
                            layoutsToShowOn,
                            /* isVisibilityManuallyControlled= */ false,
                            captureResourceIdSupplier,
                            mToolbarLayout.getProgressBar());
            layoutManager.addSceneOverlay(mOverlayCoordinator);
            mToolbarLayout.setOverlayCoordinator(mOverlayCoordinator);

            // mOverlayCoordinator needs to receive the latest yOffset and offset tags to position
            // the scene layer. It's better to request another update to avoid stale values.
            if (mBrowserControls.getControlsPosition() == ControlsPosition.TOP) {
                mTopControlsStacker.requestLayerUpdatePost(false);
            }
        }

        if (mTabStripTransitionCoordinator == null) {
            mTabStripTransitionCoordinator =
                    maybeInitializeTabStripTransitionCoordinator(
                            mToolbarLayout,
                            mControlContainer,
                            mTabObscuringHandler,
                            mDesktopWindowStateManager,
                            mTabStripTransitionDelegateSupplier,
                            tabStripTransitionHandler);
        }
    }

    private static @Nullable TabStripTransitionCoordinator
            maybeInitializeTabStripTransitionCoordinator(
                    ToolbarLayout toolbarLayout,
                    ControlContainer controlContainer,
                    TabObscuringHandler tabObscuringHandler,
                    @Nullable DesktopWindowStateManager desktopWindowStateManager,
                    OneshotSupplier<TabStripTransitionDelegate> tabStripTransitionDelegateSupplier,
                    TabStripTransitionHandler tabStripTransitionHandler) {
        int tabStripHeightResource = toolbarLayout.getTabStripHeightFromResource();
        if (tabStripHeightResource <= 0) return null;

        var coordinator =
                new TabStripTransitionCoordinator(
                        controlContainer,
                        tabStripHeightResource,
                        tabObscuringHandler,
                        desktopWindowStateManager,
                        tabStripTransitionDelegateSupplier,
                        tabStripTransitionHandler);
        toolbarLayout.getContext().registerComponentCallbacks(coordinator);
        toolbarLayout.setTabStripTransitionCoordinator(coordinator);
        return coordinator;
    }

    /**
     * Sets the {@link ExtensionsToolbarCoordinator}.
     *
     * <p>This method is not called if the extension toolbar is unavailable. If it is called, it is
     * after native initialization.
     *
     * @param extensionsToolbarCoordinator The {@link ExtensionsToolbarCoordinator} to be set.
     */
    public void setExtensionsToolbarCoordinator(
            ExtensionsToolbarCoordinator extensionsToolbarCoordinator) {
        mToolbarLayout.setExtensionsToolbarCoordinator(extensionsToolbarCoordinator);
    }

    /** Returns the color of the hairline drawn underneath the toolbar. */
    public @ColorInt int getToolbarHairlineColor() {
        return mToolbarLayout.getToolbarHairlineColor();
    }

    /**
     * @param toolbarColorObserver The observer that observes toolbar color change.
     */
    public void setToolbarColorObserver(ToolbarColorObserver toolbarColorObserver) {
        mToolbarColorObserverManager.setToolbarColorObserver(toolbarColorObserver);
    }

    /**
     * @see View#addOnAttachStateChangeListener(View.OnAttachStateChangeListener)
     */
    public void addOnAttachStateChangeListener(View.OnAttachStateChangeListener listener) {
        mToolbarLayout.addOnAttachStateChangeListener(listener);
    }

    /**
     * @see View#removeOnAttachStateChangeListener(View.OnAttachStateChangeListener)
     */
    public void removeOnAttachStateChangeListener(View.OnAttachStateChangeListener listener) {
        mToolbarLayout.removeOnAttachStateChangeListener(listener);
    }

    /**
     * Set the Bookmark Bar height supplier for the current TopToolbarOverlayCoordinator.
     *
     * @param bookmarkBarHeightSupplier Supplier of the current Bookmark Bar height.
     */
    public void setBookmarkBarHeightSupplier(
            @Nullable Supplier<Integer> bookmarkBarHeightSupplier) {
        if (mOverlayCoordinator == null) return;
        mOverlayCoordinator.setBookmarkBarHeightSupplier(bookmarkBarHeightSupplier);
    }

    /** Cleans up any code as necessary. */
    @SuppressWarnings("NullAway")
    public void destroy() {
        if (mOverlayCoordinator != null) {
            mOverlayCoordinator.destroy();
            mOverlayCoordinator = null;
        }
        mToolbarLayout.destroy();

        if (mOptionalButtonController != null) {
            mOptionalButtonController.destroy();
            mOptionalButtonController = null;
        }

        if (mSigninButtonCoordinator != null) {
            mSigninButtonCoordinator.destroy();
            mSigninButtonCoordinator = null;
        }

        if (mReloadButtonCoordinator != null) {
            mReloadButtonCoordinator.destroy();
            mReloadButtonCoordinator = null;
        }

        if (mAppMenuButtonHelperSupplier != null) {
            mAppMenuButtonHelperSupplier = null;
        }
        if (mTabCountSupplier != null) {
            mTabCountSupplier = null;
        }
        if (mControlContainer != null) {
            mControlContainer = null;
        }
        if (mTabStripTransitionCoordinator != null) {
            mToolbarLayout
                    .getContext()
                    .unregisterComponentCallbacks(mTabStripTransitionCoordinator);
            mTabStripTransitionCoordinator.destroy();
            mTabStripTransitionCoordinator = null;
        }
        mTopControlsStacker.removeControl(this);
    }

    /**
     * Notifies whether the progress bar is being drawn by WebContents for back forward transition
     * UI.
     */
    public void setShowingProgressBarForBackForwardTransition(
            boolean showingProgressBarForBackForwardTransition) {
        mToolbarLayout.setShowingProgressBarForBackForwardTransition(
                showingProgressBarForBackForwardTransition);
    }

    @Override
    public void disableMenuButton() {
        mMenuButtonCoordinator.disableMenuButton();
        mToolbarLayout.onMenuButtonDisabled();
    }

    /**
     * @return The wrapper for the browsing mode toolbar's menu button.
     */
    public @Nullable MenuButton getMenuButtonWrapper() {
        return mMenuButtonCoordinator.getMenuButton();
    }

    @Override
    public ToolbarProgressBar getProgressBar() {
        return mToolbarLayout.getProgressBar();
    }

    @Override
    public int getPrimaryColor() {
        return mToolbarLayout.getToolbarDataProvider().getPrimaryColor();
    }

    @Override
    public void getPositionRelativeToContainer(View containerView, int[] position) {
        mToolbarLayout.getPositionRelativeToContainer(containerView, position);
    }

    /**
     * Gives inheriting classes the chance to respond to
     * {@link FindToolbar} state changes.
     * @param showing Whether or not the {@code FindToolbar} will be showing.
     */
    public void handleFindLocationBarStateChange(boolean showing) {
        mToolbarLayout.handleFindLocationBarStateChange(showing);
        if (mTabStripTransitionCoordinator != null) {
            if (showing) {
                mFindToolbarToken =
                        mTabStripTransitionCoordinator.requestDeferTabStripTransitionToken();
            } else {
                mTabStripTransitionCoordinator.releaseTabStripToken(mFindToolbarToken);
                mFindToolbarToken = TokenHolder.INVALID_TOKEN;
            }
        }
    }

    /** Sets whether the urlbar should be hidden on first page load. */
    public void setUrlBarHidden(boolean hidden) {
        mToolbarLayout.setUrlBarHidden(hidden);
    }

    /** Tells the Toolbar to update what buttons it is currently displaying. */
    public void updateButtonVisibility() {
        mToolbarLayout.updateButtonVisibility();
        if (mOptionalButtonController != null) {
            mOptionalButtonController.updateButtonVisibility();
        }
        if (mSigninButtonCoordinator != null) {
            mSigninButtonCoordinator.updateButtonVisibility();
        }
    }

    /**
     * Gets the {@link AdaptiveToolbarButtonVariant} of the currently shown optional button. {@code
     * AdaptiveToolbarButtonVariant.NONE} is returned if there's no visible optional button.
     *
     * @return A value from {@link AdaptiveToolbarButtonVariant}.
     */
    public @AdaptiveToolbarButtonVariant int getCurrentOptionalButtonVariant() {
        return mOptionalButtonController.getCurrentButtonVariant();
    }

    /**
     * Sets the delegate for the optional button.
     *
     * @param delegate The {@link OptionalBrowsingModeButtonController.Delegate}.
     */
    public void setOptionalButtonDelegate(OptionalBrowsingModeButtonController.Delegate delegate) {
        mOptionalButtonController.setDelegate(delegate);
    }

    @Override
    public void updateReloadButtonVisibility(boolean isReloading) {
        mNtpLoadingSupplier.set(isReloading);
    }

    /**
     * Gives inheriting classes the chance to update the visual status of the
     * bookmark button.
     * @param isBookmarked Whether or not the current tab is already bookmarked.
     * @param editingAllowed Whether or not bookmarks can be modified (added, edited, or removed).
     */
    public void updateBookmarkButton(boolean isBookmarked, boolean editingAllowed) {
        mToolbarLayout.updateBookmarkButton(isBookmarked, editingAllowed);
    }

    /**
     * Triggered when the current tab or model has changed.
     *
     * <p>As there are cases where you can select a model with no tabs (i.e. having incognito tabs
     * but no normal tabs will still allow you to select the normal model), this should not
     * guarantee that the model's current tab is non-null.
     */
    public void onTabOrModelChanged() {
        mToolbarLayout.onTabOrModelChanged();
        mControlContainer.onTabOrModelChanged(mToolbarLayout.isIncognito());
    }

    /**
     * For extending classes to override and carry out the changes related with the primary color
     * for the current tab changing.
     */
    public void onPrimaryColorChanged(boolean shouldAnimate) {
        mToolbarLayout.onPrimaryColorChanged(shouldAnimate);
    }

    /**
     * Sets whether a title should be shown within the Toolbar.
     *
     * @param showTitle Whether a title should be shown.
     */
    public void setShowTitle(boolean showTitle) {
        getLocationBar().setShowTitle(showTitle);
    }

    /**
     * Sets the icon drawable that the close button in the toolbar (if any) should show, or hides
     * it if {@code drawable} is {@code null}.
     */
    public void setCloseButtonImageResource(@Nullable Drawable drawable) {
        mToolbarLayout.setCloseButtonImageResource(drawable);
    }

    /**
     * Sets custom actions visibility of the custom tab toolbar.
     *
     * @param isVisible true if should be visible, false if should be hidden.
     */
    public void setCustomActionsVisibility(boolean isVisible) {
        mToolbarLayout.setCustomActionsVisibility(isVisible);
    }

    /**
     * Adds a custom action button to the toolbar layout, if it is supported.
     *
     * @param drawable The icon for the button.
     * @param description The content description for the button.
     * @param listener The {@link View.OnClickListener} to use for clicks to the button.
     * @param {@link ButtonType} of the button.
     */
    public void addCustomActionButton(
            Drawable drawable, String description, View.OnClickListener listener, int type) {
        mToolbarLayout.addCustomActionButton(drawable, description, listener, type);
    }

    /**
     * Updates the visual appearance of a custom action button in the toolbar layout,
     * if it is supported.
     * @param index The index of the button.
     * @param drawable The icon for the button.
     * @param description The content description for the button.
     */
    public void updateCustomActionButton(int index, Drawable drawable, String description) {
        mToolbarLayout.updateCustomActionButton(index, drawable, description);
    }

    @Override
    public int getTabStripHeight() {
        if (mTabStripTransitionCoordinator != null) {
            return mTabStripTransitionCoordinator.getTabStripHeight();
        }
        return mToolbarLayout.getTabStripHeightFromResource();
    }

    /** Triggered when the content view for the specified tab has changed. */
    public void onTabContentViewChanged() {
        mToolbarLayout.onTabContentViewChanged();
    }

    /** Triggered when the page of the specified tab had painted something non-empty. */
    public void onDidFirstVisuallyNonEmptyPaint() {
        mToolbarLayout.onDidFirstVisuallyNonEmptyPaint();
    }

    @Override
    public CaptureReadinessResult isReadyForTextureCapture() {
        return mToolbarLayout.isReadyForTextureCapture();
    }

    /**
     * @param attached Whether or not the web content is attached to the view heirarchy.
     */
    public void setContentAttached(boolean attached) {
        mToolbarLayout.setContentAttached(attached);
    }

    /**
     * Gives inheriting classes the chance to show or hide the TabSwitcher mode of this toolbar.
     *
     * @param inTabSwitcherMode Whether or not TabSwitcher mode should be shown or hidden.
     */
    public void setTabSwitcherMode(boolean inTabSwitcherMode) {
        mToolbarLayout.setTabSwitcherMode(inTabSwitcherMode);
    }

    /**
     * Gives inheriting classes the chance to update their state when the TabSwitcher transition has
     * finished.
     */
    public void onTabSwitcherTransitionFinished() {
        mToolbarLayout.onTabSwitcherTransitionFinished();
    }

    /**
     * Gives inheriting classes the chance to update themselves based on default search engine
     * changes.
     */
    public void onDefaultSearchEngineChanged() {
        mToolbarLayout.onDefaultSearchEngineChanged();
    }

    @Override
    public void getLocationBarContentRect(Rect outRect) {
        mToolbarLayout.getLocationBarContentRect(outRect);
    }

    @Override
    public void setTextureCaptureMode(boolean textureMode) {
        mToolbarLayout.setTextureCaptureMode(textureMode);
    }

    @Override
    public boolean shouldIgnoreSwipeGesture() {
        return mToolbarLayout.shouldIgnoreSwipeGesture();
    }

    /**
     * Triggered when the URL input field has gained or lost focus.
     * @param hasFocus Whether the URL field has gained focus.
     */
    public void onUrlFocusChange(boolean hasFocus) {
        mToolbarLayout.onUrlFocusChange(hasFocus);
        if (mTabStripTransitionCoordinator != null) {
            mTabStripTransitionCoordinator.onUrlFocusChange(hasFocus);
        }
    }

    public void onUrlAnimationFinished(boolean hasFocus) {
        if (mTabStripTransitionCoordinator != null) {
            mTabStripTransitionCoordinator.onUrlAnimationFinished(hasFocus);
        }
    }

    /** Notified when a navigation to a different page has occurred. */
    public void onNavigatedToDifferentPage() {
        mToolbarLayout.onNavigatedToDifferentPage();
    }

    public void onPageLoadStopped() {
        mControlContainer.onPageLoadStopped();
    }

    /** Finish any toolbar animations. */
    public void finishAnimations() {
        mToolbarLayout.finishAnimations();
    }

    /**
     * @return {@link LocationBar} object this {@link ToolbarLayout} contains.
     */
    public LocationBar getLocationBar() {
        return mToolbarLayout.getLocationBar();
    }

    @Override
    public int getHeight() {
        return mToolbarLayout.getHeight();
    }

    /**
     * Sets the id of a view after which the toolbar should be visited in accessibility traversal.
     *
     * @param viewId The view id which the toolbar should be traversed after.
     */
    public void setAccessibilityTraversalAfter(int viewId) {
        mToolbarLayout.setAccessibilityTraversalAfter(viewId);
    }

    /** Gets the id of a view after which the toolbar is visited in accessibility traversal. */
    public int getAccessibilityTraversalAfter() {
        return mToolbarLayout.getAccessibilityTraversalAfter();
    }

    /** Returns the {@link OptionalBrowsingModeButtonController}. */
    public @Nullable OptionalBrowsingModeButtonController getOptionalButtonControllerForTesting() {
        return mOptionalButtonController;
    }

    /** Returns the {@link ToolbarLayout} that constitutes the toolbar. */
    public ToolbarLayout getToolbarLayoutForTesting() {
        return mToolbarLayout;
    }

    /** Returns the {@link TabStripTransitionCoordinator}. */
    public @Nullable TabStripTransitionCoordinator getTabStripTransitionCoordinator() {
        return mTabStripTransitionCoordinator;
    }

    @Override
    public void setBrowsingModeHairlineVisibility(boolean isVisible) {
        mToolbarLayout.setHairlineVisibility(isVisible);
    }

    @Override
    public boolean isBrowsingModeToolbarVisible() {
        return mToolbarLayout.getVisibility() == View.VISIBLE;
    }

    @Override
    public View removeLocationBarView() {
        assert mToolbarLayout instanceof ToolbarPhone
                : "Location bar removal logic is only supported on phones";
        mToolbarLayout.removeView(mLocationBarView);
        return mLocationBarView;
    }

    @Override
    public void restoreLocationBarView() {
        assert mToolbarLayout instanceof ToolbarPhone
                : "Location bar restore logic is only supported on phones";
        mToolbarLayout.addView(mLocationBarView, mIndexOfLocationBarInToolbar);
    }

    @Override
    public void onCaptureSizeUpdated() {
        if (mBrowserControls.getControlsPosition() != ControlsPosition.TOP
                || mOverlayCoordinator == null) {
            return;
        }

        updateSceneLayerYOffset(/* includeMinHeightBoundary= */ true);
    }

    public void onTransitionStart() {
        mToolbarLayout.onTransitionStart();
    }

    public void onTransitionEnd() {
        mToolbarLayout.onTransitionEnd();
    }

    /** Requests keyboard focus on the toolbar row. */
    public void requestFocus() {
        mToolbarLayout.requestKeyboardFocus();
    }

    /** Returns true if the toolbar contains keyboard focus. */
    public boolean containsKeyboardFocus() {
        return mToolbarLayout.getFocusedChild() != null;
    }

    public void onContentViewScrollingStateChanged(boolean scrolling) {
        mControlContainer.onContentViewScrollingStateChanged(scrolling);
    }

    // TopControlLayer implementation:

    @Override
    public @TopControlType int getTopControlType() {
        return TopControlType.TOOLBAR;
    }

    @Override
    public int getTopControlHeight() {
        return mControlContainer.getToolbarHeight();
    }

    @Override
    public int getTopControlVisibility() {
        if (mBrowserControls.getControlsPosition() != ControlsPosition.TOP) {
            return TopControlVisibility.HIDDEN;
        }
        return TopControlVisibility.VISIBLE;
    }

    @Override
    public void onTopControlLayerHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        if (mBrowserControls.getControlsPosition() != ControlsPosition.TOP) {
            return;
        }

        // TODO(crbug.com/448641122): This may be better placed in the hairline view itself.
        // If this layer is at the bottom of the stacker, the hairline should be visible.
        mIsHairlineVisible = mTopControlsStacker.isLayerAtBottom(getTopControlType());
        mToolbarLayout.setHairlineVisibility(mIsHairlineVisible);
    }

    @Override
    public void updateOffsetTag(@Nullable BrowserControlsOffsetTagsInfo offsetTagsInfo) {
        if (mBrowserControls.getControlsPosition() != ControlsPosition.TOP
                || mOverlayCoordinator == null) {
            return;
        }

        mOverlayCoordinator.setOffsetTagInfo(offsetTagsInfo);
    }

    @Override
    public void onBrowserControlsOffsetUpdate(int layerYOffset, boolean reachRestingPosition) {
        if (mBrowserControls.getControlsPosition() != ControlsPosition.TOP) {
            return;
        }

        mLayerYOffset = layerYOffset;
        if (mOverlayCoordinator != null) {
            updateSceneLayerYOffset(/* includeMinHeightBoundary= */ false);
        }

        // Skip the layout params in non-resting position to avoid trigger layout during browser
        // controls reposition.
        if (reachRestingPosition) {
            if (ChromeFeatureList.sToolbarSnapshotRefactor.isEnabled()) {
                mControlContainer.mutateToolbarLayoutParams().topMargin = 0;
            } else {
                mControlContainer.mutateToolbarLayoutParams().topMargin = getTabStripHeight();
            }
        }
    }

    @Override
    public void prepForHeightAdjustmentAnimation(int latestYOffset) {
        if (mBrowserControls.getControlsPosition() != ControlsPosition.TOP
                || mOverlayCoordinator == null) {
            return;
        }

        // Remove the offset tag on animation starts, so the toolbar does not set the yOffset
        // while the compositor moves the layer with offset tags.
        mOverlayCoordinator.setOffsetTagInfo(null);
        updateSceneLayerYOffset(/* includeMinHeightBoundary= */ true);
    }

    // In compositor, the position of the toolbar depends on the capture. As of Nov 2025, the
    // capture includes everything in control container, including the top margin, which represents
    // the size of the tab strip.
    // To place the toolbar at its desired position, we have to subtract the diffs of the capture a
    // nd the toolbar, in order to put the toolbar at the desired yOffset.
    private void updateSceneLayerYOffset(boolean includeMinHeightBoundary) {
        // Edge case: When Chrome launches on NTP, the browser controls might not dispatch
        // a valid yOffset for the toolbar. If the capture size changes (e.g. resize screen), we
        // we will not have a valid yOffset.
        // Pull the height from top as a fallback value from top controls stacker.
        if (mLayerYOffset == UNSPECIFIED_TOOLBAR_OFFSET) {
            mLayerYOffset = mTopControlsStacker.getHeightFromLayerToTop(getTopControlType());
        }

        int captureHeight = mControlContainer.getToolbarCaptureHeight();

        // The |diff| is the offset we need to move the toolbar scene layer upward to have the
        // Toolbar show at the correct spot. The current math here is to reduce the capture size
        // with toolbar height and hairline height.
        int diff = 0;

        int tabStripHeight = mToolbarLayout.getTabStripHeightFromResource();
        if (ChromeFeatureList.sAndroidTabstripStartupCaptureBugFix.isEnabled()
                && !ChromeFeatureList.sToolbarSnapshotRefactor.isEnabled()
                && captureHeight == 0
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(mToolbarLayout.getContext())) {
            // TODO(peilinwang): This is a temporary fix for https://crbug.com/504438014. The
            // toolbar for tablets is the only UI element where the height of its capture is
            // different from its actual height, and sometimes the offset and capture don't get
            // updated at the same time. When that happens, the composited layer will appear to be
            // in the wrong place. Remove this, and the math for capture diff for the progress bar
            // and toolbar after ToolbarSnapshotRefactor launches. This assumes that the tabstrip is
            // always positioned right above the toolbar.
            diff = tabStripHeight;
        } else {
            // When switching omnibox from bottom to top, the toolbar capture size may not have been
            // updated yet (e.g. captureHeight=1 while toolbarLayoutHeight=137). Using a stale
            // capture height produces a large negative diff that pushes the cc layer below the
            // toolbar, creating a "ghost view". Only compute diff when capture height is at least
            // as large as the toolbar, indicating the capture is up-to-date.
            int toolbarLayoutHeight = mControlContainer.getToolbarHeight();
            int hairlineHeight = mControlContainer.getToolbarHairlineHeight();
            int controlContainerHeightExcludingTabStrip =
                    mControlContainer.getControlContainerHeightExcludingTabStrip();
            // The control container can be larger than toolbarLayoutHeight + tabstrip height, e.g.
            // when the fusebox is visible. The capture does not always include this expanded height
            // but when it does, we need to account for it to avoid over-translating by the extra
            // height.
            int maxControlContainerHeightMeasurement =
                    Math.max(controlContainerHeightExcludingTabStrip, toolbarLayoutHeight);
            int minControlContainerHeightMeasurement =
                    Math.min(controlContainerHeightExcludingTabStrip, toolbarLayoutHeight);
            if (captureHeight >= maxControlContainerHeightMeasurement + tabStripHeight
                    && mTabStripTransitionCoordinator != null) {
                // Capture includes extra height; use the full height.
                diff = captureHeight - maxControlContainerHeightMeasurement - hairlineHeight;
            } else if (captureHeight >= minControlContainerHeightMeasurement) {
                diff = captureHeight - minControlContainerHeightMeasurement - hairlineHeight;
            }
        }

        // As toolbar hairline is part of the capture, there are times we need to hide the hairline
        // (e.g. When browser controls are forced hidden) to avoid the capture showing up.
        // We want to shift the scene layer upward by a little so the hairline is not revealed.
        // This is not a perfect fix since hairline might still cover the content when scene layer
        // is fully scrolled off.
        // TODO(crbug.com/448641122): Let hairline layer owns the adjustment logic.
        int hairlineAdjustment = 0;
        if (shouldHideHairlineInSceneLayer(includeMinHeightBoundary)) {
            hairlineAdjustment = -mControlContainer.getToolbarHairlineHeight();
        }

        assertNonNull(mOverlayCoordinator).setYOffset(mLayerYOffset - diff + hairlineAdjustment);
    }

    /**
     * Returns whether the toolbar scene layer should be shifted up to keep the hairline hidden.
     *
     * @param includeMinHeightBoundary Whether to include content offsets exactly at the top
     *     controls min-height boundary. When false, the adjustment only applies after the content
     *     offset has moved past the min-height boundary.
     */
    private boolean shouldHideHairlineInSceneLayer(boolean includeMinHeightBoundary) {
        // When mIsHairlineVisible is false, the hairline view is hidden and therefore is not
        // drawn into the toolbar capture. With no hairline in the capture there is nothing to
        // hide, so no scene layer adjustment is needed.
        if (!mIsHairlineVisible) return false;
        if (mBrowserControls.getBrowserVisibilityDelegate().get() == BrowserControlsState.HIDDEN) {
            return true;
        }

        int topControlsMinHeight = mBrowserControls.getTopControlsMinHeight();
        int topControlsHairlineHeight = mBrowserControls.getTopControlsHairlineHeight();
        int contentOffset = mBrowserControls.getContentOffset();
        return (includeMinHeightBoundary || contentOffset > topControlsMinHeight)
                && BrowserControlsUtils.shouldContentOffsetHideTopControlsHairline(
                        contentOffset, topControlsMinHeight, topControlsHairlineHeight);
    }
}
