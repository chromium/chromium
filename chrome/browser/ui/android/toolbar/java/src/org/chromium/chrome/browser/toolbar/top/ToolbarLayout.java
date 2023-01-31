// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ProgressBar;

import androidx.annotation.CallSuper;
import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.lifetime.DestroyChecker;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownScrollListener;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.HomeButton;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup.HistoryDelegate;
import org.chromium.chrome.browser.toolbar.top.ToolbarTablet.OfflineDownloader;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.ToolbarColorObserver;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.UrlExpansionObserver;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.url.GURL;

import java.util.function.BooleanSupplier;

/**
 * Layout class that contains the base shared logic for manipulating the toolbar component. For
 * interaction that are not from Views inside Toolbar hierarchy all interactions should be done
 * through {@link Toolbar} rather than using this class directly.
 */
public abstract class ToolbarLayout
        extends FrameLayout implements Destroyable, TintObserver, ThemeColorObserver,
                                       OmniboxSuggestionsDropdownScrollListener {
    private Callback<Runnable> mInvalidator;
    private @Nullable ToolbarColorObserver mToolbarColorObserver;

    protected final ObserverList<UrlExpansionObserver> mUrlExpansionObservers =
            new ObserverList<>();
    private final int[] mTempPosition = new int[2];

    private final ColorStateList mDefaultTint;

    private ToolbarDataProvider mToolbarDataProvider;
    private ToolbarTabController mToolbarTabController;

    @Nullable
    protected ToolbarProgressBar mProgressBar;
    @Nullable
    protected BooleanSupplier mPartnerHomepageEnabledSupplier;

    private boolean mNativeLibraryReady;
    private boolean mUrlHasFocus;

    private long mFirstDrawTimeMs;

    private boolean mFindInPageToolbarShowing;

    private ThemeColorProvider mThemeColorProvider;
    private MenuButtonCoordinator mMenuButtonCoordinator;
    private AppMenuButtonHelper mAppMenuButtonHelper;

    private TopToolbarOverlayCoordinator mOverlayCoordinator;

    protected final DestroyChecker mDestroyChecker;

    /**
     * Basic constructor for {@link ToolbarLayout}.
     */
    public ToolbarLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mDefaultTint = ThemeUtils.getThemedToolbarIconTint(getContext(), false);
        mDestroyChecker = new DestroyChecker();

        addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View view, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (isNativeLibraryReady() && mProgressBar.getParent() != null) {
                    mProgressBar.initializeAnimation();
                }

                // Since this only needs to happen once, remove this listener from the view.
                removeOnLayoutChangeListener(this);
            }
        });
    }

    /**
     * Initialize the external dependencies required for view interaction.
     * @param toolbarDataProvider The provider for toolbar data.
     * @param tabController       The controller that handles interactions with the tab.
     * @param menuButtonCoordinator Coordinator for interacting with the MenuButton.
     * @param historyDelegate Delegate used to display navigation history.
     * @param partnerHomepageEnabledSupplier A supplier of a boolean indicating that partner
     *        homepage is enabled.
     * @param offlineDownloader Triggers downloading an offline page.
     */
    @CallSuper
    public void initialize(ToolbarDataProvider toolbarDataProvider,
            ToolbarTabController tabController, MenuButtonCoordinator menuButtonCoordinator,
            HistoryDelegate historyDelegate, BooleanSupplier partnerHomepageEnabledSupplier,
            OfflineDownloader offlineDownloader) {
        mToolbarDataProvider = toolbarDataProvider;
        mToolbarTabController = tabController;
        mMenuButtonCoordinator = menuButtonCoordinator;
        mPartnerHomepageEnabledSupplier = partnerHomepageEnabledSupplier;
        mProgressBar = createProgressBar();
    }

    /** @param overlay The coordinator for the texture version of the top toolbar. */
    void setOverlayCoordinator(TopToolbarOverlayCoordinator overlay) {
        mOverlayCoordinator = overlay;
        mOverlayCoordinator.setIsAndroidViewVisible(getVisibility() == View.VISIBLE);
    }

    @Override
    public void setVisibility(int visibility) {
        super.setVisibility(visibility);
        if (mOverlayCoordinator != null) {
            mOverlayCoordinator.setIsAndroidViewVisible(visibility == View.VISIBLE);
        }
    }

    /**
     * @param appMenuButtonHelper The helper for managing menu button interactions.
     */
    void setAppMenuButtonHelper(AppMenuButtonHelper appMenuButtonHelper) {
        mAppMenuButtonHelper = appMenuButtonHelper;
    }

    // TODO(pnoland, https://crbug.com/865801): Move this from ToolbarLayout to forthcoming
    // BrowsingModeToolbarCoordinator.
    public void setLocationBarCoordinator(LocationBarCoordinator locationBarCoordinator) {}

    /** Cleans up any code as necessary. */
    @Override
    public void destroy() {
        mDestroyChecker.destroy();

        if (mThemeColorProvider != null) {
            mThemeColorProvider.removeTintObserver(this);
            mThemeColorProvider.removeThemeColorObserver(this);
            mThemeColorProvider = null;
        }

        if (mToolbarColorObserver != null) {
            mToolbarColorObserver = null;
        }
    }

    /**
     * @param urlExpansionObserver The observer that observes URL expansion progress change.
     */
    void addUrlExpansionObserver(UrlExpansionObserver urlExpansionObserver) {
        mUrlExpansionObservers.addObserver(urlExpansionObserver);
    }

    /**
     * @param urlExpansionObserver The observer that observes URL expansion progress change.
     */
    void removeUrlExpansionObserver(UrlExpansionObserver urlExpansionObserver) {
        mUrlExpansionObservers.removeObserver(urlExpansionObserver);
    }

    /**
     * @param toolbarColorObserver The observer that observes toolbar color change.
     */
    void setToolbarColorObserver(@NonNull ToolbarColorObserver toolbarColorObserver) {
        mToolbarColorObserver = toolbarColorObserver;
    }

    /**
     * @param themeColorProvider The {@link ThemeColorProvider} used for tinting the toolbar
     *                           buttons.
     */
    void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addTintObserver(this);
        mThemeColorProvider.addThemeColorObserver(this);
    }

    /**
     * @return The tint the toolbar buttons should use.
     */
    protected ColorStateList getTint() {
        return mThemeColorProvider == null ? mDefaultTint : mThemeColorProvider.getTint();
    }

    @Override
    public void onTintChanged(ColorStateList tint, @BrandedColorScheme int brandedColorScheme) {}

    @Override
    public void onThemeColorChanged(int color, boolean shouldAnimate) {}

    /**
     * Set the height that the progress bar should be.
     * @return The progress bar height in px.
     */
    int getProgressBarHeight() {
        return getResources().getDimensionPixelSize(R.dimen.toolbar_progress_bar_height);
    }

    /**
     * @return A progress bar for Chrome to use.
     */
    private ToolbarProgressBar createProgressBar() {
        return new ToolbarProgressBar(getContext(), getProgressBarHeight(), this, false);
    }

    /**
     * TODO comment
     */
    @CallSuper
    protected void onMenuButtonDisabled() {}

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        // Initialize the provider to an empty version to avoid null checking everywhere.
        mToolbarDataProvider = new ToolbarDataProvider() {
            @Override
            public boolean isIncognito() {
                return false;
            }

            @Override
            public boolean isInOverviewAndShowingOmnibox() {
                return false;
            }

            @Override
            public boolean shouldShowLocationBarInOverviewMode() {
                return false;
            }

            @Override
            public Profile getProfile() {
                return null;
            }

            @Override
            public Tab getTab() {
                return null;
            }

            @Override
            public String getCurrentUrl() {
                return "";
            }

            @Override
            public GURL getCurrentGurl() {
                return GURL.emptyGURL();
            }

            @Override
            public UrlBarData getUrlBarData() {
                return UrlBarData.EMPTY;
            }

            @Override
            public NewTabPageDelegate getNewTabPageDelegate() {
                return NewTabPageDelegate.EMPTY;
            }

            @Override
            public int getPrimaryColor() {
                return 0;
            }

            @Override
            public boolean isUsingBrandColor() {
                return false;
            }

            @Override
            public @DrawableRes int getSecurityIconResource(boolean isTablet) {
                return 0;
            }

            @Override
            public boolean isPaintPreview() {
                return false;
            }
        };
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        try (TraceEvent e = TraceEvent.scoped("ToolbarLayout.onMeasure")) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        try (TraceEvent e = TraceEvent.scoped("ToolbarLayout.onLayout")) {
            super.onLayout(changed, left, top, right, bottom);
        }
    }

    @Override
    public void draw(Canvas canvas) {
        try (TraceEvent e = TraceEvent.scoped("ToolbarLayout.draw")) {
            super.draw(canvas);
        }
    }

    /**
     * Quick getter for LayoutParams for a View inside a FrameLayout.
     * @param view {@link View} to fetch the layout params for.
     * @return {@link LayoutParams} the given {@link View} is currently using.
     */
    FrameLayout.LayoutParams getFrameLayoutParams(View view) {
        return ((FrameLayout.LayoutParams) view.getLayoutParams());
    }

    /**
     *  This function handles native dependent initialization for this class.
     */
    protected void onNativeLibraryReady() {
        mNativeLibraryReady = true;
        if (mProgressBar.getParent() != null) mProgressBar.initializeAnimation();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        // TODO(crbug.com/1021877): lazy initialize progress bar.
        // Posting adding progress bar can prevent parent view group from using a stale children
        // count, which can cause a crash in rare cases.
        post(this::addProgressBarToHierarchy);
    }

    /**
     * @return The view containing the menu button and menu button badge.
     */
    MenuButtonCoordinator getMenuButtonCoordinator() {
        return mMenuButtonCoordinator;
    }

    @VisibleForTesting
    void setMenuButtonCoordinatorForTesting(MenuButtonCoordinator menuButtonCoordinator) {
        mMenuButtonCoordinator = menuButtonCoordinator;
    }

    /**
     * @return The {@link ProgressBar} this layout uses.
     */
    @Nullable
    protected ToolbarProgressBar getProgressBar() {
        return mProgressBar;
    }

    void getPositionRelativeToContainer(View containerView, int[] position) {
        ViewUtils.getRelativeDrawPosition(containerView, this, position);
    }

    /**
     * @return Whether or not the native library is loaded and ready.
     */
    boolean isNativeLibraryReady() {
        return mNativeLibraryReady;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        recordFirstDrawTime();
    }

    /**
     * Add the toolbar's progress bar to the view hierarchy.
     */
    void addProgressBarToHierarchy() {
        ViewGroup controlContainer = (ViewGroup) getRootView().findViewById(R.id.control_container);
        int progressBarPosition =
                UiUtils.insertAfter(controlContainer, mProgressBar, (View) getParent());
        assert progressBarPosition >= 0;
        mProgressBar.setProgressBarContainer(controlContainer);
    }

    /**
     * @return The provider for toolbar related data.
     */
    public ToolbarDataProvider getToolbarDataProvider() {
        return mToolbarDataProvider;
    }

    /**
     * Sets the {@link Invalidator} that will be called when the toolbar attempts to invalidate the
     * drawing surface.  This will give the object that registers as the host for the
     * {@link Invalidator} a chance to defer the actual invalidate to sync drawing.
     * @param invalidator An {@link Invalidator} instance.
     */
    void setInvalidatorCallback(Callback<Runnable> invalidator) {
        mInvalidator = invalidator;
    }

    /**
     * Triggers a paint but allows the {@link Invalidator} set by
     * {@link #setPaintInvalidator(Invalidator)} to decide when to actually invalidate.
     * @param client A {@link Invalidator.Client} instance that wants to be invalidated.
     */
    protected void triggerPaintInvalidate(Runnable clientInvalidator) {
        mInvalidator.onResult(clientInvalidator);
    }

    /**
     * Gives inheriting classes the chance to respond to
     * {@link FindToolbar} state changes.
     * @param showing Whether or not the {@code FindToolbar} will be showing.
     */
    void handleFindLocationBarStateChange(boolean showing) {
        mFindInPageToolbarShowing = showing;
    }

    /**
     * Sets the OnClickListener that will be notified when the TabSwitcher button is pressed.
     * @param listener The callback that will be notified when the TabSwitcher button is pressed.
     */
    void setOnTabSwitcherClickHandler(OnClickListener listener) {}

    /**
     * Sets the OnLongClickListener that will be notified when the TabSwitcher button is long
     *         pressed.
     * @param listener The callback that will be notified when the TabSwitcher button is long
     *         pressed.
     */
    void setOnTabSwitcherLongClickHandler(OnLongClickListener listener) {}

    /**
     * Sets the OnClickListener that will be notified when the bookmark button is pressed.
     * @param listener The callback that will be notified when the bookmark button is pressed.
     */
    void setBookmarkClickHandler(OnClickListener listener) {}

    /**
     * Sets the OnClickListener to notify when the close button is pressed in a custom tab.
     * @param listener The callback that will be notified when the close button is pressed.
     */
    protected void setCustomTabCloseClickHandler(OnClickListener listener) {}

    /**
     * Sets whether the urlbar should be hidden on first page load.
     */
    protected void setUrlBarHidden(boolean hide) {}

    /**
     * Tells the Toolbar to update what buttons it is currently displaying.
     */
    void updateButtonVisibility() {}

    /**
     * Gives inheriting classes the chance to update the visibility of the
     * back button.
     * @param canGoBack Whether or not the current tab has any history to go back to.
     */
    void updateBackButtonVisibility(boolean canGoBack) {}

    /**
     * Gives inheriting classes the chance to update the visibility of the
     * forward button.
     * @param canGoForward Whether or not the current tab has any history to go forward to.
     */
    void updateForwardButtonVisibility(boolean canGoForward) {}

    /**
     * Gives inheriting classes the chance to update the visibility of the
     * reload button.
     * @param isReloading Whether or not the current tab is loading.
     */
    void updateReloadButtonVisibility(boolean isReloading) {}

    /**
     * Gives inheriting classes the chance to update the visual status of the
     * bookmark button.
     * @param isBookmarked Whether or not the current tab is already bookmarked.
     * @param editingAllowed Whether or not bookmarks can be modified (added, edited, or removed).
     */
    void updateBookmarkButton(boolean isBookmarked, boolean editingAllowed) {}

    /**
     * Gives inheriting classes the chance to do the necessary UI operations after Chrome is
     * restored to a previously saved state.
     */
    void onStateRestored() {}

    /**
     * Gives inheriting classes the chance to update home button UI if home button preference is
     * changed.
     * @param homeButtonEnabled Whether or not home button is enabled in preference.
     */
    void onHomeButtonUpdate(boolean homeButtonEnabled) {}

    /**
     * Triggered when the current tab or model has changed.
     * <p>
     * As there are cases where you can select a model with no tabs (i.e. having incognito
     * tabs but no normal tabs will still allow you to select the normal model), this should
     * not guarantee that the model's current tab is non-null.
     */
    void onTabOrModelChanged() {}

    /**
     * For extending classes to override and carry out the changes related with the primary color
     * for the current tab changing.
     */
    public void onPrimaryColorChanged(boolean shouldAnimate) {}

    /**
     * Sets the icon drawable that the close button in the toolbar (if any) should show, or hides
     * it if {@code drawable} is {@code null}.
     */
    protected void setCloseButtonImageResource(@Nullable Drawable drawable) {}

    /**
     * Adds a custom action button to the toolbar layout, if it is supported.
     * @param drawable The icon for the button.
     * @param description The content description for the button.
     * @param listener The {@link OnClickListener} to use for clicks to the button.
     */
    protected void addCustomActionButton(
            Drawable drawable, String description, OnClickListener listener) {
        // This method should only be called for subclasses that override it.
        assert false;
    }

    /**
     * Updates the visual appearance of a custom action button in the toolbar layout,
     * if it is supported.
     * @param index The index of the button.
     * @param drawable The icon for the button.
     * @param description The content description for the button.
     */
    protected void updateCustomActionButton(int index, Drawable drawable, String description) {
        // This method should only be called for subclasses that override it.
        assert false;
    }

    /**
     * @return The height of the tab strip. Return 0 for toolbars that do not have a tabstrip.
     */
    protected int getTabStripHeight() {
        return getResources().getDimensionPixelSize(R.dimen.tab_strip_height);
    }

    /**
     * Triggered when the content view for the specified tab has changed.
     */
    void onTabContentViewChanged() {}

    protected CaptureReadinessResult isReadyForTextureCapture() {
        return CaptureReadinessResult.unknown(/*isReady=*/true);
    }

    boolean setForceTextureCapture(boolean forceTextureCapture) {
        return false;
    }

    void setLayoutUpdater(Runnable layoutUpdater) {}

    /**
     * @param attached Whether or not the web content is attached to the view heirarchy.
     */
    void setContentAttached(boolean attached) {}

    /**
     * Called when tab switcher mode is entered or exited.
     * @param inTabSwitcherMode Whether or not tab switcher mode is being shown or hidden.
     * @param showToolbar    Whether or not to show the normal toolbar while animating.
     * @param delayAnimation Whether or not to delay the animation until after the transition has
     *                       finished (which can be detected by a call to
     *                       {@link #onTabSwitcherTransitionFinished()}).
     */
    void setTabSwitcherMode(boolean inTabSwitcherMode, boolean showToolbar, boolean delayAnimation,
            MenuButtonCoordinator menuButtonCoordinator) {}

    /**
     * Gives inheriting classes the chance to update their state when the TabSwitcher transition has
     * finished.
     */
    void onTabSwitcherTransitionFinished() {}

    /**
     * Called when start surface state is changed.
     * @param shouldBeVisible Whether toolbar layout should be visible.
     * @param isShowingStartSurfaceHomepage Whether start surface homepage is showing.
     * @param isShowingStartSurfaceTabSwitcher Whether the StartSurface-controlled TabSwitcher is
     *         showing.
     */
    void onStartSurfaceStateChanged(boolean shouldBeVisible, boolean isShowingStartSurfaceHomepage,
            boolean isShowingStartSurfaceTabSwitcher) {}

    /**
     * Force to hide toolbar shadow.
     * @param forceHideShadow Whether toolbar shadow should be hidden.
     */
    void setForceHideShadow(boolean forceHideShadow) {}

    /**
     * Gives inheriting classes the chance to observe tab count changes.
     * @param tabCountProvider The {@link TabCountProvider} subclasses can observe.
     */
    void setTabCountProvider(TabCountProvider tabCountProvider) {}

    /**
     * Gives inheriting classes the chance to update themselves based on default search engine
     * changes.
     */
    void onDefaultSearchEngineChanged() {}

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        // Consumes mouse button events on toolbar so they don't get leaked to content layer.
        // See https://crbug.com/740855.
        if ((event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0
                && event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE) {
            int action = event.getActionMasked();
            if (action == MotionEvent.ACTION_BUTTON_PRESS
                    || action == MotionEvent.ACTION_BUTTON_RELEASE) {
                return true;
            }
        }
        return super.onGenericMotionEvent(event);
    }

    void getLocationBarContentRect(Rect outRect) {
        View container = getLocationBar().getContainerView();
        outRect.set(container.getPaddingLeft(), container.getPaddingTop(),
                container.getWidth() - container.getPaddingRight(),
                container.getHeight() - container.getPaddingBottom());
        ViewUtils.getRelativeDrawPosition(this, getLocationBar().getContainerView(), mTempPosition);
        outRect.offset(mTempPosition[0], mTempPosition[1]);
    }

    protected void setTextureCaptureMode(boolean textureMode) {}

    boolean shouldIgnoreSwipeGesture() {
        if (mUrlHasFocus || mFindInPageToolbarShowing) return true;
        return mAppMenuButtonHelper != null && mAppMenuButtonHelper.isAppMenuActive();
    }

    /**
     * @return Whether or not the url bar has focus.
     */
    boolean urlHasFocus() {
        return mUrlHasFocus;
    }

    /**
     * Triggered when the URL input field has gained or lost focus.
     * @param hasFocus Whether the URL field has gained focus.
     */
    void onUrlFocusChange(boolean hasFocus) {
        mUrlHasFocus = hasFocus;
    }

    /**
     * Keeps track of the first time the toolbar is drawn.
     */
    private void recordFirstDrawTime() {
        if (mFirstDrawTimeMs == 0) mFirstDrawTimeMs = SystemClock.elapsedRealtime();
    }

    /**
     * Returns the elapsed realtime in ms of the time at which first draw for the toolbar occurred.
     */
    long getFirstDrawTime() {
        return mFirstDrawTimeMs;
    }

    /**
     * Notified when a navigation to a different page has occurred.
     */
    protected void onNavigatedToDifferentPage() {}

    /**
     * Finish any toolbar animations.
     */
    void finishAnimations() {}

    /**
     * @return The current View showing in the Tab.
     */
    View getCurrentTabView() {
        Tab tab = mToolbarDataProvider.getTab();
        if (tab != null) {
            return tab.getView();
        }
        return null;
    }

    /**
     * @return Whether or not the toolbar is incognito.
     */
    protected boolean isIncognito() {
        return mToolbarDataProvider.isIncognito();
    }

    /**
     * @return {@link LocationBar} object this {@link ToolbarLayout} contains.
     */
    @VisibleForTesting
    public abstract LocationBar getLocationBar();

    /**
     * Navigates the current Tab back.
     * @return Whether or not the current Tab did go back.
     */
    boolean back() {
        maybeUnfocusUrlBar();
        return mToolbarTabController != null && mToolbarTabController.back();
    }

    /**
     * Navigates the current Tab forward.
     * @return Whether or not the current Tab did go forward.
     */
    boolean forward() {
        maybeUnfocusUrlBar();
        return mToolbarTabController != null ? mToolbarTabController.forward() : false;
    }

    /**
     * If the page is currently loading, this will trigger the tab to stop.  If the page is fully
     * loaded, this will trigger a refresh.
     *
     * <p>The buttons of the toolbar will be updated as a result of making this call.
     */
    void stopOrReloadCurrentTab() {
        maybeUnfocusUrlBar();
        if (mToolbarTabController != null) mToolbarTabController.stopOrReloadCurrentTab();
    }

    /**
     * Opens hompage in the current tab.
     */
    void openHomepage() {
        maybeUnfocusUrlBar();
        if (mToolbarTabController != null) mToolbarTabController.openHomepage();
    }

    private void maybeUnfocusUrlBar() {
        if (getLocationBar() != null && getLocationBar().getOmniboxStub() != null) {
            getLocationBar().getOmniboxStub().setUrlBarFocus(
                    false, null, OmniboxFocusReason.UNFOCUS);
        }
    }

    /**
     * Update the optional toolbar button, showing it if currently hidden.
     * @param buttonData Display data for the button, e.g. the Drawable and content description.
     */
    void updateOptionalButton(ButtonData buttonData) {}

    /**
     * Hide the optional toolbar button.
     */
    void hideOptionalButton() {}

    /**
     * @return Optional button view.
     */
    @VisibleForTesting
    public View getOptionalButtonViewForTesting() {
        return null;
    }

    /**
     * Sets the current TabModelSelector so the toolbar can pass it into buttons that need access to
     * it.
     */
    void setTabModelSelector(TabModelSelector selector) {}

    /**
     * @return {@link HomeButton} this {@link ToolbarLayout} contains.
     */
    public HomeButton getHomeButton() {
        return null;
    }

    /**
     * Returns whether there are any ongoing animations.
     */
    @VisibleForTesting
    public boolean isAnimationRunningForTesting() {
        return false;
    }

    /**
     * Sets the toolbar hairline color, if the toolbar has a hairline below it.
     * @param toolbarColor The toolbar color to base the hairline color on.
     */
    protected void setToolbarHairlineColor(@ColorInt int toolbarColor) {
        final ImageView shadow = getRootView().findViewById(R.id.toolbar_hairline);
        // Tests don't always set this up. TODO(https://crbug.com/1365954): Refactor this dep.
        if (shadow != null) {
            shadow.setImageTintList(ColorStateList.valueOf(getToolbarHairlineColor(toolbarColor)));
        }
    }

    /**
     * Returns the border color between the toolbar and WebContents area.
     * @param toolbarColor Toolbar color
     */
    public @ColorInt int getToolbarHairlineColor(@ColorInt int toolbarColor) {
        return ThemeUtils.getToolbarHairlineColor(getContext(), toolbarColor, isIncognito());
    }

    /**
     * Sets the {@link BrowserStateBrowserControlsVisibilityDelegate} instance the toolbar should
     * use to manipulate the visibility of browser controls; notably, "browser controls" includes
     * the toolbar itself.
     */
    public void setBrowserControlsVisibilityDelegate(
            BrowserStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate) {}

    /**
     * Notify the observer that the toolbar color is changed and pass the toolbar color to the
     * observer.
     */
    protected void notifyToolbarColorChanged(int color) {
        if (mToolbarColorObserver != null) {
            mToolbarColorObserver.onToolbarColorChanged(color);
        }
    }

    /**
     * This method sets the toolbar hairline visibility.
     * @param isHairlineVisible whether the toolbar hairline should be visible.
     */
    public void setHairlineVisibility(boolean isHairlineVisible) {
        ImageView shadow = getRootView().findViewById(R.id.toolbar_hairline);
        if(shadow != null) {
            shadow.setVisibility(isHairlineVisible ? VISIBLE : GONE);
        }
    }
}
