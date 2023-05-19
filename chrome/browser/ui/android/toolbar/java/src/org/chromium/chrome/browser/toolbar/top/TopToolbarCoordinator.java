// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewStub;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup.HistoryDelegate;
import org.chromium.chrome.browser.toolbar.top.ToolbarTablet.OfflineDownloader;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.resources.ResourceManager;

import java.util.List;
import java.util.function.BooleanSupplier;

/**
 * A coordinator for the top toolbar component.
 */
public class TopToolbarCoordinator implements Toolbar {
    /**
     * Observes toolbar URL expansion progress change.
     */
    public interface UrlExpansionObserver {
        /**
         * Notified when toolbar URL expansion progress fraction changes.
         *
         * @param fraction The toolbar expansion progress. 0 indicates that the URL bar is not
         *                   expanded. 1 indicates that the URL bar is expanded to the maximum
         *                   width.
         */
        void onUrlExpansionProgressChanged(float fraction);
    }

    /**
     * Observes toolbar color change.
     */
    public interface ToolbarColorObserver {
        /** @param color The toolbar color value. */
        void onToolbarColorChanged(int color);
    }

    /**
     * Observes toolbar alpha value change during overview mode fading animation.
     */
    public interface ToolbarAlphaInOverviewObserver {
        /** @param fraction The toolbar alpha value. */
        void onToolbarAlphaInOverviewChanged(float fraction);
    }

    public static final int TAB_SWITCHER_MODE_NORMAL_ANIMATION_DURATION_MS = 200;
    public static final int TAB_SWITCHER_MODE_GTS_ANIMATION_DURATION_MS = 150;

    private final ToolbarLayout mToolbarLayout;

    private final boolean mIsStartSurfaceRefactorEnabled;

    /**
     * The coordinator for the tab switcher mode toolbar (phones only). This will be lazily created
     * after ToolbarLayout is inflated.
     */
    private @Nullable TabSwitcherModeTTCoordinator mTabSwitcherModeCoordinator;
    /**
     * The coordinator for the start surface mode toolbar (phones only) if the StartSurface is
     * enabled. This will be lazily created after ToolbarLayout is inflated.
     */
    private @Nullable StartSurfaceToolbarCoordinator mStartSurfaceToolbarCoordinator;

    private OptionalBrowsingModeButtonController mOptionalButtonController;

    private MenuButtonCoordinator mMenuButtonCoordinator;
    private ObservableSupplier<AppMenuButtonHelper> mAppMenuButtonHelperSupplier;
    private ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;

    private ToolbarControlContainer mControlContainer;
    private Supplier<ResourceManager> mResourceManagerSupplier;
    private TopToolbarOverlayCoordinator mOverlayCoordinator;
    private boolean mStartSurfaceToolbarVisible;

    /**
     * The observer manager will receive all types of toolbar color change updates from toolbar
     * components and send the rendering toolbar color to the ToolbarColorObserver.
     */
    private ToolbarColorObserverManager mToolbarColorObserverManager;

    /**
     * Creates a new {@link TopToolbarCoordinator}.
     * @param controlContainer The {@link ToolbarControlContainer} for the containing activity.
     * @param toolbarStub The stub for the tab switcher mode toolbar.
     * @param fullscreenToolbarStub The stub for the fullscreen tab switcher mode toolbar.
     * @param toolbarLayout The {@link ToolbarLayout}.
     * @param userEducationHelper Helper class for showing in-product help text bubbles.
     * @param buttonDataProviders List of classes that wish to display an optional button in the
     *         browsing mode toolbar.
     * @param layoutStateProviderSupplier Supplier of the {@link LayoutStateProvider}.
     * @param normalThemeColorProvider The {@link ThemeColorProvider} for normal mode.
     * @param overviewThemeColorProvider The {@link ThemeColorProvider} for overview mode.
     * @param tabModelSelectorSupplier Supplier of the {@link TabModelSelector}.
     * @param homepageEnabledSupplier Supplier of whether Home button is enabled.
     * @param identityDiscController The controller of the identity disc button.
     * @param invalidatorCallback Callback that will be invoked  when the toolbar attempts to
     *        invalidate the drawing surface.  This will give the object that registers as the host
     *        for the {@link Invalidator} a chance to defer the actual invalidate to sync drawing.
     * @param identityDiscButtonSupplier Supplier of Identity Disc button.
     * @param resourceManagerSupplier A supplier of a resource manager for native textures.
     * @param isGridTabSwitcherEnabled Whether grid tab switcher is enabled via a feature flag.
     * @param isTabToGtsAnimationEnabled Whether Tab-to-GTS animation is enabled via a feature flag.
     * @param isStartSurfaceEnabled Whether start surface is enabled via a feature flag.
     * @param isTabGroupsAndroidContinuationEnabled Whether flag TabGroupsContinuationAndroid is
     *         enabled.
     * @param initializeWithIncognitoColors Whether the toolbar should be initialized with incognito
     *         colors.
     * @param startSurfaceLogoClickedCallback The callback to be notified when the logo is clicked
     *         on Start surface. On NTP, the logo is in the new tab page layout instead of the
     *         toolbar and the logo click events are processed in NewTabPageLayout. So this callback
     *         will only be called on Start surface.
     * @param constraintsSupplier Supplier for browser controls constraints.
     * @param compositorInMotionSupplier Whether there is an ongoing touch or gesture.
     * @param browserStateBrowserControlsVisibilityDelegate Used to keep controls locked when
     *         captures are stale and not able to be taken.
     * @param shouldCreateLogoInStartToolbar Whether logo should be created in Start surface
     *         toolbar. True if the logo should be created in the Start surface toolbar; False if
     *         the logo should be shown in Start surface content.
     */
    public TopToolbarCoordinator(ToolbarControlContainer controlContainer, ViewStub toolbarStub,
            ViewStub fullscreenToolbarStub, ToolbarLayout toolbarLayout,
            ToolbarDataProvider toolbarDataProvider, ToolbarTabController tabController,
            UserEducationHelper userEducationHelper, List<ButtonDataProvider> buttonDataProviders,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            ThemeColorProvider normalThemeColorProvider,
            ThemeColorProvider overviewThemeColorProvider,
            MenuButtonCoordinator browsingModeMenuButtonCoordinator,
            MenuButtonCoordinator overviewModeMenuButtonCoordinator,
            ObservableSupplier<AppMenuButtonHelper> appMenuButtonHelperSupplier,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            ObservableSupplier<Boolean> homepageEnabledSupplier,
            ButtonDataProvider identityDiscController, Callback<Runnable> invalidatorCallback,
            Supplier<ButtonData> identityDiscButtonSupplier,
            Supplier<ResourceManager> resourceManagerSupplier,
            BooleanSupplier isIncognitoModeEnabledSupplier, boolean isGridTabSwitcherEnabled,
            boolean isTabToGtsAnimationEnabled, boolean isStartSurfaceEnabled,
            boolean isTabGroupsAndroidContinuationEnabled, HistoryDelegate historyDelegate,
            BooleanSupplier partnerHomepageEnabledSupplier, OfflineDownloader offlineDownloader,
            boolean initializeWithIncognitoColors,
            Callback<LoadUrlParams> startSurfaceLogoClickedCallback,
            boolean isStartSurfaceRefactorEnabled, ObservableSupplier<Integer> constraintsSupplier,
            ObservableSupplier<Boolean> compositorInMotionSupplier,
            BrowserStateBrowserControlsVisibilityDelegate
                    browserStateBrowserControlsVisibilityDelegate,
            boolean shouldCreateLogoInStartToolbar) {
        mControlContainer = controlContainer;
        mToolbarLayout = toolbarLayout;
        mMenuButtonCoordinator = browsingModeMenuButtonCoordinator;
        mOptionalButtonController = new OptionalBrowsingModeButtonController(buttonDataProviders,
                userEducationHelper, mToolbarLayout, () -> toolbarDataProvider.getTab());
        mResourceManagerSupplier = resourceManagerSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mIsStartSurfaceRefactorEnabled = isStartSurfaceRefactorEnabled;
        mToolbarColorObserverManager =
                new ToolbarColorObserverManager(mToolbarLayout.getContext(), mToolbarLayout);
        mToolbarLayout.setToolbarColorObserver(mToolbarColorObserverManager);

        if (mToolbarLayout instanceof ToolbarPhone && isStartSurfaceEnabled) {
            mStartSurfaceToolbarCoordinator = new StartSurfaceToolbarCoordinator(toolbarStub,
                    userEducationHelper, identityDiscController, overviewThemeColorProvider,
                    overviewModeMenuButtonCoordinator, identityDiscButtonSupplier,
                    isGridTabSwitcherEnabled, isTabToGtsAnimationEnabled,
                    isTabGroupsAndroidContinuationEnabled, isIncognitoModeEnabledSupplier,
                    startSurfaceLogoClickedCallback, mIsStartSurfaceRefactorEnabled,
                    shouldCreateLogoInStartToolbar, this::onStartSurfaceToolbarTransitionFinished,
                    mToolbarColorObserverManager);
        } else if (mToolbarLayout instanceof ToolbarPhone
                || mToolbarLayout instanceof ToolbarTablet) {
            mTabSwitcherModeCoordinator = new TabSwitcherModeTTCoordinator(toolbarStub,
                    fullscreenToolbarStub, overviewModeMenuButtonCoordinator,
                    isGridTabSwitcherEnabled, isTabToGtsAnimationEnabled,
                    isIncognitoModeEnabledSupplier, mToolbarColorObserverManager);
        }
        controlContainer.setPostInitializationDependencies(this, initializeWithIncognitoColors,
                constraintsSupplier, toolbarDataProvider::getTab, compositorInMotionSupplier,
                browserStateBrowserControlsVisibilityDelegate, layoutStateProviderSupplier);
        mToolbarLayout.initialize(toolbarDataProvider, tabController, mMenuButtonCoordinator,
                historyDelegate, partnerHomepageEnabledSupplier, offlineDownloader);
        mToolbarLayout.setThemeColorProvider(normalThemeColorProvider);
        mAppMenuButtonHelperSupplier = appMenuButtonHelperSupplier;
        new OneShotCallback<>(mAppMenuButtonHelperSupplier, this::setAppMenuButtonHelper);
        homepageEnabledSupplier.addObserver((show) -> mToolbarLayout.onHomeButtonUpdate(show));
        mToolbarLayout.setInvalidatorCallback(invalidatorCallback);
    }

    /**
     * Set fullscreen GTS toolbar stub
     * @param toolbarStub stub to set.
     */
    public void setFullScreenToolbarStub(ViewStub toolbarStub) {
        if (mTabSwitcherModeCoordinator != null) {
            mTabSwitcherModeCoordinator.setFullScreenToolbarStub(toolbarStub);
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
     * <p>
     * Calling this must occur after the native library have completely loaded.
     *
     * @param layoutUpdater A {@link Runnable} used to request layout update upon scene change.
     * @param tabSwitcherClickHandler The click handler for the tab switcher button.
     * @param newTabClickHandler The click handler for the new tab button.
     * @param bookmarkClickHandler The click handler for the bookmarks button.
     * @param customTabsBackClickHandler The click handler for the custom tabs back button.
     * @param appMenuDelegate Allows interacting with the app menu.
     * @param layoutManager A {@link LayoutManager} used to watch for scene changes.
     * @param tabSupplier Supplier of the activity tab.
     * @param browserControlsStateProvider {@link BrowserControlsStateProvider} to access browser
     *                                     controls offsets.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     */
    public void initializeWithNative(Runnable layoutUpdater,
            OnClickListener tabSwitcherClickHandler, OnClickListener newTabClickHandler,
            OnClickListener bookmarkClickHandler, OnClickListener customTabsBackClickHandler,
            AppMenuDelegate appMenuDelegate, LayoutManager layoutManager,
            ObservableSupplier<Tab> tabSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            TopUiThemeColorProvider topUiThemeColorProvider) {
        assert mTabModelSelectorSupplier.get() != null;
        Callback<Integer> tabSwitcherLongClickCallback =
                menuItemId -> appMenuDelegate.onOptionsItemSelected(menuItemId, null);
        if (mTabSwitcherModeCoordinator != null) {
            mTabSwitcherModeCoordinator.setOnNewTabClickHandler(newTabClickHandler);
            mTabSwitcherModeCoordinator.setTabModelSelector(mTabModelSelectorSupplier.get());
        } else if (mStartSurfaceToolbarCoordinator != null) {
            mStartSurfaceToolbarCoordinator.setOnNewTabClickHandler(newTabClickHandler);
            mStartSurfaceToolbarCoordinator.setTabModelSelector(mTabModelSelectorSupplier.get());
            mStartSurfaceToolbarCoordinator.setTabSwitcherListener(tabSwitcherClickHandler);
            mStartSurfaceToolbarCoordinator.setOnTabSwitcherLongClickHandler(
                    StartSurfaceTabSwitcherActionMenuCoordinator.createOnLongClickListener(
                            tabSwitcherLongClickCallback));
            mStartSurfaceToolbarCoordinator.initLogoWithNative();
        }

        mToolbarLayout.setTabModelSelector(mTabModelSelectorSupplier.get());
        getLocationBar().updateVisualsForState();
        mToolbarLayout.setOnTabSwitcherClickHandler(tabSwitcherClickHandler);
        mToolbarLayout.setOnTabSwitcherLongClickHandler(
                TabSwitcherActionMenuCoordinator.createOnLongClickListener(
                        tabSwitcherLongClickCallback));
        mToolbarLayout.setBookmarkClickHandler(bookmarkClickHandler);
        mToolbarLayout.setCustomTabCloseClickHandler(customTabsBackClickHandler);
        mToolbarLayout.setLayoutUpdater(layoutUpdater);

        mToolbarLayout.onNativeLibraryReady();

        // If fullscreen is disabled, don't bother creating this overlay; only the android view will
        // ever be shown.
        if (DeviceClassManager.enableFullscreen()) {
            mOverlayCoordinator = new TopToolbarOverlayCoordinator(mToolbarLayout.getContext(),
                    layoutManager, mControlContainer::getProgressBarDrawingInfo, tabSupplier,
                    browserControlsStateProvider, mResourceManagerSupplier, topUiThemeColorProvider,
                    LayoutType.BROWSING | LayoutType.SIMPLE_ANIMATION | LayoutType.TAB_SWITCHER,
                    false);
            layoutManager.addSceneOverlay(mOverlayCoordinator);
            mToolbarLayout.setOverlayCoordinator(mOverlayCoordinator);
        }
    }

    /**
     * @param urlExpansionObserver The observer that observes URL expansion progress change.
     */
    public void addUrlExpansionObserver(UrlExpansionObserver urlExpansionObserver) {
        mToolbarLayout.addUrlExpansionObserver(urlExpansionObserver);
    }

    /**
     * @param urlExpansionObserver The observer that observes URL expansion progress change.
     */
    public void removeUrlExpansionObserver(UrlExpansionObserver urlExpansionObserver) {
        mToolbarLayout.removeUrlExpansionObserver(urlExpansionObserver);
    }

    /**
     * @param toolbarColorObserver The observer that observes toolbar color change.
     */
    public void setToolbarColorObserver(@NonNull ToolbarColorObserver toolbarColorObserver) {
        mToolbarColorObserverManager.setToolbarColorObserver(toolbarColorObserver);
    }

    /**
     * @see View#addOnAttachStateChangeListener(View.OnAttachStateChangeListener)
     */
    public void addOnAttachStateChangeListener(View.OnAttachStateChangeListener listener) {
        mToolbarLayout.addOnAttachStateChangeListener(listener);
    }

    /**
     * Cleans up any code as necessary.
     */
    public void destroy() {
        if (mOverlayCoordinator != null) {
            mOverlayCoordinator.destroy();
            mOverlayCoordinator = null;
        }
        mToolbarLayout.destroy();
        if (mTabSwitcherModeCoordinator != null) {
            mTabSwitcherModeCoordinator.destroy();
        } else if (mStartSurfaceToolbarCoordinator != null) {
            mStartSurfaceToolbarCoordinator.destroy();
        }

        if (mOptionalButtonController != null) {
            mOptionalButtonController.destroy();
            mOptionalButtonController = null;
        }

        if (mAppMenuButtonHelperSupplier != null) {
            mAppMenuButtonHelperSupplier = null;
        }
        if (mTabModelSelectorSupplier != null) {
            mTabModelSelectorSupplier = null;
        }
        if (mControlContainer != null) {
            mControlContainer = null;
        }
    }

    @Override
    public void disableMenuButton() {
        mMenuButtonCoordinator.disableMenuButton();
        mToolbarLayout.onMenuButtonDisabled();
    }

    /**
     * @return The wrapper for the browsing mode toolbar's menu button.
     */
    public MenuButton getMenuButtonWrapper() {
        return mMenuButtonCoordinator.getMenuButton();
    }

    @Nullable
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
     * Sets the {@link Invalidator} that will be called when the toolbar attempts to invalidate the
     * drawing surface.  This will give the object that registers as the host for the
     * {@link Invalidator} a chance to defer the actual invalidate to sync drawing.
     * @param invalidator An {@link Invalidator} instance.
     */
    public void setInvalidatorCallback(Callback<Runnable> callback) {
        mToolbarLayout.setInvalidatorCallback(callback);
    }

    /**
     * Gives inheriting classes the chance to respond to
     * {@link FindToolbar} state changes.
     * @param showing Whether or not the {@code FindToolbar} will be showing.
     */
    public void handleFindLocationBarStateChange(boolean showing) {
        mToolbarLayout.handleFindLocationBarStateChange(showing);
    }

    /**
     * Sets whether the urlbar should be hidden on first page load.
     */
    public void setUrlBarHidden(boolean hidden) {
        mToolbarLayout.setUrlBarHidden(hidden);
    }

    /**
     * Tells the Toolbar to update what buttons it is currently displaying.
     */
    public void updateButtonVisibility() {
        mToolbarLayout.updateButtonVisibility();
        mOptionalButtonController.updateButtonVisibility();
    }

    /**
     * Gets the {@link AdaptiveToolbarButtonVariant} of the currently shown optional button. {@code
     * AdaptiveToolbarButtonVariant.NONE} is returned if there's no visible optional button.
     * @return A value from {@link AdaptiveToolbarButtonVariant}.
     */
    public @AdaptiveToolbarButtonVariant int getCurrentOptionalButtonVariant() {
        return mOptionalButtonController.getCurrentButtonVariant();
    }

    /**
     * Gives inheriting classes the chance to update the visibility of the
     * back button.
     * @param canGoBack Whether or not the current tab has any history to go back to.
     */
    public void updateBackButtonVisibility(boolean canGoBack) {
        mToolbarLayout.updateBackButtonVisibility(canGoBack);
    }

    /**
     * Gives inheriting classes the chance to update the visibility of the
     * forward button.
     * @param canGoForward Whether or not the current tab has any history to go forward to.
     */
    public void updateForwardButtonVisibility(boolean canGoForward) {
        mToolbarLayout.updateForwardButtonVisibility(canGoForward);
    }

    @Override
    public void updateReloadButtonVisibility(boolean isReloading) {
        mToolbarLayout.updateReloadButtonVisibility(isReloading);
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
     * Gives inheriting classes the chance to respond to accessibility state changes.
     * @param enabled Whether or not accessibility is enabled.
     */
    public void onAccessibilityStatusChanged(boolean enabled) {
        if (mTabSwitcherModeCoordinator != null) {
            mTabSwitcherModeCoordinator.onAccessibilityStatusChanged(enabled);
        } else if (mStartSurfaceToolbarCoordinator != null) {
            mStartSurfaceToolbarCoordinator.onAccessibilityStatusChanged(enabled);
        }
    }

    /**
     * Gives inheriting classes the chance to do the necessary UI operations after Chrome is
     * restored to a previously saved state.
     */
    public void onStateRestored() {
        mToolbarLayout.onStateRestored();
    }

    /**
     * Triggered when the current tab or model has changed.
     * <p>
     * As there are cases where you can select a model with no tabs (i.e. having incognito
     * tabs but no normal tabs will still allow you to select the normal model), this should
     * not guarantee that the model's current tab is non-null.
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
     * Adds a custom action button to the toolbar layout, if it is supported.
     * @param drawable The icon for the button.
     * @param description The content description for the button.
     * @param listener The {@link View.OnClickListener} to use for clicks to the button.
     */
    public void addCustomActionButton(
            Drawable drawable, String description, View.OnClickListener listener) {
        mToolbarLayout.addCustomActionButton(drawable, description, listener);
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
        return mToolbarLayout.getTabStripHeight();
    }

    /**
     * Triggered when the content view for the specified tab has changed.
     */
    public void onTabContentViewChanged() {
        mToolbarLayout.onTabContentViewChanged();
    }

    @Override
    public CaptureReadinessResult isReadyForTextureCapture() {
        return mToolbarLayout.isReadyForTextureCapture();
    }

    @Override
    public boolean setForceTextureCapture(boolean forceTextureCapture) {
        return mToolbarLayout.setForceTextureCapture(forceTextureCapture);
    }

    /**
     * @param attached Whether or not the web content is attached to the view heirarchy.
     */
    public void setContentAttached(boolean attached) {
        mToolbarLayout.setContentAttached(attached);
    }

    /**
     * Gives inheriting classes the chance to show or hide the TabSwitcher mode of this toolbar.
     * @param inTabSwitcherMode Whether or not TabSwitcher mode should be shown or hidden.
     */
    public void setTabSwitcherMode(boolean inTabSwitcherMode) {
        mToolbarLayout.setTabSwitcherMode(inTabSwitcherMode);
        if (mTabSwitcherModeCoordinator != null) {
            mTabSwitcherModeCoordinator.setTabSwitcherMode(inTabSwitcherMode);
        } else if (mStartSurfaceToolbarCoordinator != null) {
            // Do nothing. Already handled by onStartSurfaceStateChanged.
        }
    }

    /**
     * Gives inheriting classes the chance to update their state when the TabSwitcher transition has
     * finished.
     */
    public void onTabSwitcherTransitionFinished() {
        mToolbarLayout.onTabSwitcherTransitionFinished();
    }

    /**
     * Gives inheriting classes the chance to observe tab count changes.
     * @param tabCountProvider The {@link TabCountProvider} subclasses can observe.
     */
    public void setTabCountProvider(TabCountProvider tabCountProvider) {
        mToolbarLayout.setTabCountProvider(tabCountProvider);
        if (mTabSwitcherModeCoordinator != null) {
            mTabSwitcherModeCoordinator.setTabCountProvider(tabCountProvider);
        }
        if (mStartSurfaceToolbarCoordinator != null) {
            mStartSurfaceToolbarCoordinator.setTabCountProvider(tabCountProvider);
        }
    }

    /**
     * @param provider The provider used to determine incognito state.
     */
    public void setIncognitoStateProvider(IncognitoStateProvider provider) {
        if (mTabSwitcherModeCoordinator != null) {
            mTabSwitcherModeCoordinator.setIncognitoStateProvider(provider);
        } else if (mStartSurfaceToolbarCoordinator != null) {
            mStartSurfaceToolbarCoordinator.setIncognitoStateProvider(provider);
        }
        mToolbarColorObserverManager.setIncognitoStateProvider(provider);
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
    }

    /**
     * Returns the elapsed realtime in ms of the time at which first draw for the toolbar occurred.
     */
    public long getFirstDrawTime() {
        return mToolbarLayout.getFirstDrawTime();
    }

    /**
     * Notified when a navigation to a different page has occurred.
     */
    public void onNavigatedToDifferentPage() {
        mToolbarLayout.onNavigatedToDifferentPage();
    }

    /**
     * Force to hide toolbar shadow.
     * @param forceHideShadow Whether toolbar shadow should be hidden.
     *
     * TODO(crbug.com/1202994): change to token-based access
     */
    public void setForceHideShadow(boolean forceHideShadow) {
        mToolbarLayout.setForceHideShadow(forceHideShadow);
    }

    /**
     * Finish any toolbar animations.
     */
    public void finishAnimations() {
        mToolbarLayout.finishAnimations();
    }

    /**
     * @return {@link LocationBar} object this {@link ToolbarLayout} contains.
     */
    public LocationBar getLocationBar() {
        return mToolbarLayout.getLocationBar();
    }

    /**
     * Update the start surface toolbar state.
     * @param newState New Start Surface State.
     * @param requestToShow Whether or not request showing the start surface toolbar.
     */
    public void updateStartSurfaceToolbarState(@Nullable @StartSurfaceState Integer newState,
            boolean requestToShow, @Nullable @LayoutType Integer newLayoutType) {
        if (mStartSurfaceToolbarCoordinator == null
                || mToolbarLayout.getToolbarDataProvider() == null) {
            return;
        }
        assert (mIsStartSurfaceRefactorEnabled && newLayoutType != null)
                || (!mIsStartSurfaceRefactorEnabled && newState != null);
        mStartSurfaceToolbarCoordinator.onStartSurfaceStateChanged(
                newState, requestToShow, newLayoutType);
        updateToolbarLayoutVisibility();
        updateButtonVisibility();
    }

    /**
     * Triggered when the offset of start surface header view is changed.
     * @param verticalOffset The start surface header view's offset.
     */
    public void onStartSurfaceHeaderOffsetChanged(int verticalOffset) {
        if (mStartSurfaceToolbarCoordinator != null) {
            mStartSurfaceToolbarCoordinator.onStartSurfaceHeaderOffsetChanged(verticalOffset);
            updateToolbarLayoutVisibility();
        }
    }

    private void updateToolbarLayoutVisibility() {
        assert mStartSurfaceToolbarCoordinator != null;
        // We continue to show the browsing mode toolbar while the tab switcher is fading in or out.
        // Once this transition finishes, onStartSurfaceToolbarFinishedShowing() will reset the
        // browsing mode toolbar's visibility to the correct value.
        boolean showToolbar = mStartSurfaceToolbarCoordinator.shouldShowRealSearchBox()
                || (isShowingStartSurfaceTabSwitcher() && !mStartSurfaceToolbarVisible);
        mToolbarLayout.onStartSurfaceStateChanged(showToolbar,
                mStartSurfaceToolbarCoordinator.isOnHomepage(), isShowingStartSurfaceTabSwitcher());
    }

    private boolean isShowingStartSurfaceTabSwitcher() {
        return mStartSurfaceToolbarCoordinator != null
                && mStartSurfaceToolbarCoordinator.isShowingTabSwitcher();
    }

    private void onStartSurfaceToolbarTransitionFinished(boolean nowShowing) {
        mStartSurfaceToolbarVisible = nowShowing;
        updateToolbarLayoutVisibility();
    }

    @Override
    public int getHeight() {
        return mToolbarLayout.getHeight();
    }

    /**
     * Sets the highlight on the new tab button shown during overview mode.
     * @param highlight If the new tab button should be highlighted.
     */
    public void setNewTabButtonHighlight(boolean highlight) {
        if (mTabSwitcherModeCoordinator != null) {
            mTabSwitcherModeCoordinator.setNewTabButtonHighlight(highlight);
        } else if (mStartSurfaceToolbarCoordinator != null) {
            mStartSurfaceToolbarCoordinator.setNewTabButtonHighlight(highlight);
        }
    }

    /**
     * @return A {@link TopToolbarInteractabilityManager} which allows non toolbar clients to toggle
     *         the interactability of elements present in the top toolbar.
     */
    public @NonNull TopToolbarInteractabilityManager getTopToolbarInteractabilityManager() {
        return mStartSurfaceToolbarCoordinator != null
                ? mStartSurfaceToolbarCoordinator.getTopToolbarInteractabilityManager()
                : mTabSwitcherModeCoordinator.getTopToolbarInteractabilityManager();
    }

    /** Returns the {@link OptionalBrowsingModeButtonController}. */
    @VisibleForTesting
    public OptionalBrowsingModeButtonController getOptionalButtonControllerForTesting() {
        return mOptionalButtonController;
    }

    /** Returns the {@link ToolbarLayout} that constitutes the toolbar. */
    @VisibleForTesting
    public ToolbarLayout getToolbarLayoutForTesting() {
        return mToolbarLayout;
    }

    /** Returns the {@link StartSurfaceToolbarCoordinator}. */
    @VisibleForTesting
    public StartSurfaceToolbarCoordinator getStartSurfaceToolbarForTesting() {
        return mStartSurfaceToolbarCoordinator;
    }

    @Override
    public void setBrowsingModeHairlineVisibility(boolean isVisible) {
        mToolbarLayout.setHairlineVisibility(isVisible);
    }

    @Override
    public boolean isBrowsingModeToolbarVisible() {
        return mToolbarLayout.getVisibility() == View.VISIBLE;
    }

    public void onTransitionStart() {
        mToolbarLayout.onTransitionStart();
    }

    public void onTransitionEnd() {
        mToolbarLayout.onTransitionEnd();
    }
}
