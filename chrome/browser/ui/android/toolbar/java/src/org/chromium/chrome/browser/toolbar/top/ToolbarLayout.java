// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Build.VERSION;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ProgressBar;

import androidx.annotation.CallSuper;
import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.TooltipCompat;

import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.lifetime.DestroyChecker;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.toolbar.back_button.BackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.reload_button.ReloadButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup.HistoryDelegate;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.ToolbarColorObserver;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.UrlExpansionObserver;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.util.MotionEventUtils;
import org.chromium.ui.util.TokenHolder;
import org.chromium.url.GURL;

/**
 * Layout class that contains the base shared logic for manipulating the toolbar component. For
 * interaction that are not from Views inside Toolbar hierarchy all interactions should be done
 * through {@link Toolbar} rather than using this class directly.
 */
@NullMarked
public abstract class ToolbarLayout extends FrameLayout
        implements Destroyable, TintObserver, ThemeColorObserver {
    private @Nullable ToolbarColorObserver mToolbarColorObserver;

    protected final ObserverList<UrlExpansionObserver> mUrlExpansionObservers =
            new ObserverList<>();
    private final int[] mTempPosition = new int[2];

    private final ColorStateList mDefaultTint;

    private ToolbarDataProvider mToolbarDataProvider;
    private ToolbarTabController mToolbarTabController;

    protected ToolbarProgressBar mProgressBar;

    private boolean mNativeLibraryReady;
    private boolean mUrlHasFocus;
    private boolean mFindInPageToolbarShowing;

    protected ThemeColorProvider mThemeColorProvider;
    private MenuButtonCoordinator mMenuButtonCoordinator;
    private @Nullable AppMenuButtonHelper mAppMenuButtonHelper;

    private ToggleTabStackButtonCoordinator mTabSwitcherButtonCoordinator;

    private @Nullable TopToolbarOverlayCoordinator mOverlayCoordinator;

    private @Nullable BrowserStateBrowserControlsVisibilityDelegate
            mBrowserControlsVisibilityDelegate;
    private int mShowBrowserControlsToken = TokenHolder.INVALID_TOKEN;

    private @Nullable TabStripTransitionCoordinator mTabStripTransitionCoordinator;
    private int mTabStripTransitionToken = TokenHolder.INVALID_TOKEN;

    protected final DestroyChecker mDestroyChecker;

    /** Set if the progress bar is being drawn by WebContents for back forward transition. */
    private boolean mShowingProgressBarForBackForwardTransition;

    /** Caches the color for the toolbar hairline. */
    private @ColorInt int mToolbarHairlineColor;

    private ImageView mToolbarShadow;

    /** Basic constructor for {@link ToolbarLayout}. */
    public ToolbarLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mDefaultTint = ThemeUtils.getThemedToolbarIconTint(getContext(), false);
        mDestroyChecker = new DestroyChecker();
    }

    /**
     * Initialize the external dependencies required for view interaction.
     *
     * @param toolbarDataProvider The provider for toolbar data.
     * @param tabController The controller that handles interactions with the tab.
     * @param menuButtonCoordinator Coordinator for interacting with the MenuButton.
     * @param historyDelegate Delegate used to display navigation history.
     * @param userEducationHelper Helper for user education flows.
     * @param trackerSupplier Provides a {@link Tracker} when available.
     */
    @CallSuper
    @Initializer
    public void initialize(
            ToolbarDataProvider toolbarDataProvider,
            ToolbarTabController tabController,
            MenuButtonCoordinator menuButtonCoordinator,
            ToggleTabStackButtonCoordinator tabSwitcherButtonCoordinator,
            HistoryDelegate historyDelegate,
            UserEducationHelper userEducationHelper,
            ObservableSupplier<Tracker> trackerSupplier,
            ToolbarProgressBar progressBar,
            @Nullable ReloadButtonCoordinator reloadButtonCoordinator,
            @Nullable BackButtonCoordinator backButtonCoordinator) {
        mToolbarDataProvider = toolbarDataProvider;
        mToolbarTabController = tabController;
        mMenuButtonCoordinator = menuButtonCoordinator;
        mTabSwitcherButtonCoordinator = tabSwitcherButtonCoordinator;
        mProgressBar = progressBar;
    }

    /**
     * @param overlay The coordinator for the texture version of the top toolbar.
     */
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
    @Initializer
    public void setLocationBarCoordinator(LocationBarCoordinator locationBarCoordinator) {}

    /** Cleans up any code as necessary. */
    @Override
    @SuppressWarnings("NullAway")
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
    public void setToolbarColorObserver(ToolbarColorObserver toolbarColorObserver) {
        mToolbarColorObserver = toolbarColorObserver;
    }

    /**
     * @param themeColorProvider The {@link ThemeColorProvider} used for tinting the toolbar
     *     buttons.
     */
    @Initializer
    void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addTintObserver(this);
        mThemeColorProvider.addThemeColorObserver(this);
    }

    /**
     * @return The tint the toolbar buttons should use.
     */
    protected @Nullable ColorStateList getTint() {
        return mThemeColorProvider == null ? mDefaultTint : mThemeColorProvider.getTint();
    }

    protected ImageView getToolbarShadow() {
        return mToolbarShadow;
    }

    /**
     * Notifies whether the progress bar is being drawn by WebContents for back forward transition
     * UI.
     */
    public final void setShowingProgressBarForBackForwardTransition(
            boolean showingProgressBarForBackForwardTransition) {
        if (mShowingProgressBarForBackForwardTransition
                == showingProgressBarForBackForwardTransition) return;

        mShowingProgressBarForBackForwardTransition = showingProgressBarForBackForwardTransition;
        mProgressBar.setVisibility(
                mShowingProgressBarForBackForwardTransition ? View.GONE : View.VISIBLE);
        updateShadowVisibility();
    }

    /** Update the visibility of the toolbar shadow. */
    protected void updateShadowVisibility() {
        boolean shouldDrawShadow = shouldDrawShadow();
        int shadowVisibility = shouldDrawShadow ? View.VISIBLE : View.INVISIBLE;

        if (mToolbarShadow != null && mToolbarShadow.getVisibility() != shadowVisibility) {
            mToolbarShadow.setVisibility(shadowVisibility);
        }
    }

    /**
     * @return Whether the toolbar shadow should be drawn.
     */
    protected boolean shouldDrawShadow() {
        return !mShowingProgressBarForBackForwardTransition;
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {}

    @Override
    public void onThemeColorChanged(@ColorInt int color, boolean shouldAnimate) {}

    /** TODO comment */
    @CallSuper
    protected void onMenuButtonDisabled() {}

    /**
     * Set hover tooltip text for buttons shared between phones and tablets. @TODO: Remove and use
     * the method in UiUtils.java instead once JaCoCo issue is resolved.
     */
    protected void setTooltipText(View button, @Nullable String text) {
        if (button != null && VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            TooltipCompat.setTooltipText(button, text);
        }
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        // Initialize the provider to an empty version to avoid null checking everywhere.
        mToolbarDataProvider =
                new ToolbarDataProvider() {
                    @Override
                    public boolean isIncognito() {
                        return false;
                    }

                    @Override
                    public boolean isIncognitoBranded() {
                        return false;
                    }

                    @Override
                    public boolean isOffTheRecord() {
                        return false;
                    }

                    @Override
                    public @Nullable Profile getProfile() {
                        return null;
                    }

                    @Override
                    public @Nullable Tab getTab() {
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
                    public @ColorInt int getPrimaryColor() {
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

                    @Override
                    public void addToolbarDataProviderObserver(
                            ToolbarDataProvider.Observer observer) {}

                    @Override
                    public void removeToolbarDataProviderObserver(
                            ToolbarDataProvider.Observer observer) {}
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

    /** This function handles native dependent initialization for this class. */
    protected void onNativeLibraryReady() {
        mNativeLibraryReady = true;
    }

    @Override
    @Initializer
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        mToolbarShadow = getRootView().findViewById(R.id.toolbar_hairline);
        updateShadowVisibility();
    }

    /**
     * @return The view containing the menu button and menu button badge.
     */
    MenuButtonCoordinator getMenuButtonCoordinator() {
        return mMenuButtonCoordinator;
    }

    ToggleTabStackButtonCoordinator getTabSwitcherButtonCoordinator() {
        return mTabSwitcherButtonCoordinator;
    }

    void setMenuButtonCoordinatorForTesting(MenuButtonCoordinator menuButtonCoordinator) {
        mMenuButtonCoordinator = menuButtonCoordinator;
    }

    /**
     * @return The {@link ProgressBar} this layout uses.
     */
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

    /**
     * @return The provider for toolbar related data.
     */
    public ToolbarDataProvider getToolbarDataProvider() {
        return mToolbarDataProvider;
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
     * Sets the OnClickListener that will be notified when the bookmark button is pressed.
     *
     * @param listener The callback that will be notified when the bookmark button is pressed.
     */
    void setBookmarkClickHandler(OnClickListener listener) {}

    /**
     * Sets the OnClickListener to notify when the close button is pressed in a custom tab.
     * @param listener The callback that will be notified when the close button is pressed.
     */
    protected void setCustomTabCloseClickHandler(OnClickListener listener) {}

    /** Sets whether the urlbar should be hidden on first page load. */
    protected void setUrlBarHidden(boolean hide) {}

    /** Tells the Toolbar to update what buttons it is currently displaying. */
    void updateButtonVisibility() {}

    /**
     * Gives inheriting classes the chance to update the visibility of the forward button.
     *
     * @param canGoForward Whether or not the current tab has any history to go forward to.
     */
    void updateForwardButtonVisibility(boolean canGoForward) {}

    /**
     * Gives inheriting classes the chance to update the visual status of the bookmark button.
     *
     * @param isBookmarked Whether or not the current tab is already bookmarked.
     * @param editingAllowed Whether or not bookmarks can be modified (added, edited, or removed).
     */
    void updateBookmarkButton(boolean isBookmarked, boolean editingAllowed) {}

    /**
     * Gives inheriting classes the chance to update home button UI if home button's enable
     * preference is changed.
     *
     * @param homeButtonEnabled Whether or not home button is enabled in preference.
     */
    void onHomeButtonIsEnabledUpdate(boolean homeButtonEnabled) {}

    /**
     * Gives inheriting classes the chance to update home button UI if the current homepage is set
     * to something other than the NTP.
     *
     * @param isHomepageNonNtp Whether the current homepage is something other than the NTP.
     */
    // TODO(crbug.com/407554279): Usage will be added in follow-up CLs related to the NTP
    // customization toolbar button.
    void onHomepageIsNonNtpUpdate(boolean isHomepageNonNtp) {}

    /**
     * Triggered when the current tab or model has changed.
     *
     * <p>As there are cases where you can select a model with no tabs (i.e. having incognito tabs
     * but no normal tabs will still allow you to select the normal model), this should not
     * guarantee that the model's current tab is non-null.
     */
    void onTabOrModelChanged() {}

    /**
     * For extending classes to override and carry out the changes related with the primary color
     * for the current tab changing.
     */
    public void onPrimaryColorChanged(boolean shouldAnimate) {}

    /**
     * Sets the icon drawable that the close button in the toolbar (if any) should show, or hides it
     * if {@code drawable} is {@code null}.
     */
    protected void setCloseButtonImageResource(@Nullable Drawable drawable) {}

    /**
     * Sets custom actions visibility of the custom tab toolbar, if it is supported.
     *
     * @param isVisible true if should be visible, false if should be hidden.
     */
    protected void setCustomActionsVisibility(boolean isVisible) {}

    /**
     * Adds a custom action button to the toolbar layout, if it is supported.
     *
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
     * Return the height of the tab strip from the layout resource. Return 0 for toolbars that do
     * not have a tab strip.
     */
    protected int getTabStripHeightFromResource() {
        return getResources().getDimensionPixelSize(R.dimen.tab_strip_height);
    }

    /** Triggered when the content view for the specified tab has changed. */
    void onTabContentViewChanged() {}

    /** Triggered when the page of the specified tab had painted something non-empty. */
    public void onDidFirstVisuallyNonEmptyPaint() {}

    protected abstract CaptureReadinessResult isReadyForTextureCapture();

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
    void setTabSwitcherMode(boolean inTabSwitcherMode) {}

    /**
     * Gives inheriting classes the chance to update their state when the TabSwitcher transition has
     * finished.
     */
    void onTabSwitcherTransitionFinished() {}

    /**
     * Gives inheriting classes the chance to observe tab count changes.
     *
     * @param tabCountSupplier The observable supplier subclasses can observe.
     */
    void setTabCountSupplier(ObservableSupplier<Integer> tabCountSupplier) {}

    /**
     * Gives inheriting classes the chance to update themselves based on default search engine
     * changes.
     */
    void onDefaultSearchEngineChanged() {}

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        // Consumes mouse/trackpad button events on toolbar so they don't get leaked to content
        // layer. See https://crbug.com/740855 (mouse) and https://crbug.com/384916573 (trackpad).
        if (MotionEventUtils.isMouseEvent(event) || MotionEventUtils.isTrackpadEvent(event)) {
            int action = event.getActionMasked();
            if (action == MotionEvent.ACTION_BUTTON_PRESS
                    || action == MotionEvent.ACTION_BUTTON_RELEASE
                    || action == MotionEvent.ACTION_SCROLL) {
                return true;
            }
        }
        return super.onGenericMotionEvent(event);
    }

    void getLocationBarContentRect(Rect outRect) {
        View container = getLocationBar().getContainerView();
        outRect.set(
                container.getPaddingLeft(),
                container.getPaddingTop(),
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

    /** Notified when a navigation to a different page has occurred. */
    protected void onNavigatedToDifferentPage() {}

    /** Finish any toolbar animations. */
    void finishAnimations() {}

    /**
     * @return The current View showing in the Tab.
     */
    @Nullable View getCurrentTabView() {
        Tab tab = mToolbarDataProvider.getTab();
        if (tab != null) {
            return tab.getView();
        }
        return null;
    }

    /**
     * TODO(crbug.com/350654700): clean up usages and remove isIncognito.
     *
     * @return Whether or not the toolbar is incognito.
     * @deprecated Use {@link #isIncognitoBranded()} or {@link #isOffTheRecord()}.
     */
    @Deprecated
    protected boolean isIncognito() {
        return mToolbarDataProvider.isIncognito();
    }

    /**
     * @return Whether or not the toolbar is incognito branded.
     * @see {@link ToolbarDataProvider#isIncognitoBranded()}
     */
    protected boolean isIncognitoBranded() {
        return mToolbarDataProvider.isIncognitoBranded();
    }

    /**
     * @return Whether or not the toolbar is off the record.
     * @see {@link ToolbarDataProvider#isOffTheRecord()}
     */
    protected boolean isOffTheRecord() {
        return mToolbarDataProvider.isOffTheRecord();
    }

    /**
     * @return {@link LocationBar} object this {@link ToolbarLayout} contains.
     */
    @VisibleForTesting
    public abstract LocationBar getLocationBar();

    /**
     * Navigates the current Tab forward.
     *
     * @return Whether or not the current Tab did go forward.
     */
    protected boolean forward(int metaState, String reportingTagPrefix) {
        maybeUnfocusUrlBar();
        if (mToolbarTabController == null) return false;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_META_CLICK_HISTORY_NAVIGATION)) {
            boolean hasControl = (metaState & KeyEvent.META_CTRL_ON) != 0;
            boolean hasShift = (metaState & KeyEvent.META_SHIFT_ON) != 0;
            if (hasControl && hasShift) {
                // Holding ALT is allowed as well (reference desktop behavior).

                // Note on recording user actions: "forward" is recorded regardless of whether it
                // was successful. See
                // https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/top/ToolbarTablet.java;l=196;drc=14aab80e079b7db3e85a8302da6d660bafeddfbc
                RecordUserAction.record(reportingTagPrefix + "InNewForegroundTab");
                return mToolbarTabController.forwardInNewTab(/* foregroundNewTab= */ true);
            } else if (hasControl) {
                RecordUserAction.record(reportingTagPrefix + "InNewBackgroundTab");
                return mToolbarTabController.forwardInNewTab(/* foregroundNewTab= */ false);
            } else if (hasShift) {
                RecordUserAction.record(reportingTagPrefix + "InNewForegroundWindow");
                return mToolbarTabController.forwardInNewWindow();
            } else {
                RecordUserAction.record(reportingTagPrefix);
                return mToolbarTabController.forward();
            }
        } else {
            RecordUserAction.record(reportingTagPrefix);
            return mToolbarTabController.forward();
        }
    }

    private void maybeUnfocusUrlBar() {
        if (getLocationBar() != null && getLocationBar().getOmniboxStub() != null) {
            getLocationBar()
                    .getOmniboxStub()
                    .setUrlBarFocus(false, null, OmniboxFocusReason.UNFOCUS);
        }
    }

    /**
     * Update the optional toolbar button, showing it if currently hidden.
     *
     * @param buttonData Display data for the button, e.g. the Drawable and content description.
     */
    protected void updateOptionalButton(ButtonData buttonData) {}

    /** Hide the optional toolbar button. */
    void hideOptionalButton() {}

    /**
     * @return Optional button view.
     */
    public @Nullable View getOptionalButtonViewForTesting() {
        return null;
    }

    /** Returns whether there are any ongoing animations. */
    public boolean isAnimationRunningForTesting() {
        return false;
    }

    /**
     * Sets the toolbar hairline color, if the toolbar has a hairline below it.
     *
     * @param toolbarColor The toolbar color to base the hairline color on.
     */
    protected void setToolbarHairlineColor(@ColorInt int toolbarColor) {
        final ImageView shadow = getRootView().findViewById(R.id.toolbar_hairline);
        // Tests don't always set this up. TODO(crbug.com/40866629): Refactor this dep.
        if (shadow != null) {
            mToolbarHairlineColor = computeToolbarHairlineColor(toolbarColor);
            shadow.setImageTintList(ColorStateList.valueOf(mToolbarHairlineColor));
        }
    }

    /** Returns the color of the hairline drawn underneath the toolbar. */
    public @ColorInt int getToolbarHairlineColor() {
        return mToolbarHairlineColor;
    }

    /**
     * Returns the border color between the toolbar and WebContents area.
     *
     * @param toolbarColor Toolbar color
     */
    private @ColorInt int computeToolbarHairlineColor(@ColorInt int toolbarColor) {
        return ThemeUtils.getToolbarHairlineColor(
                getContext(), toolbarColor, mToolbarDataProvider.isIncognitoBranded());
    }

    /**
     * Sets the {@link BrowserStateBrowserControlsVisibilityDelegate} instance the toolbar should
     * use to manipulate the visibility of browser controls; notably, "browser controls" includes
     * the toolbar itself.
     */
    public void setBrowserControlsVisibilityDelegate(
            BrowserStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate) {
        mBrowserControlsVisibilityDelegate = controlsVisibilityDelegate;
    }

    // TODO(crbug.com/41484813): Rework the API if this method is called by multiple clients.
    protected void keepControlsShownForAnimation() {
        // isShown() being false implies that the toolbar isn't visible. We don't want to force it
        // back into visibility just so that we can show an animation.
        if (!isShown()) return;

        if (mBrowserControlsVisibilityDelegate != null) {
            mShowBrowserControlsToken =
                    mBrowserControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                            mShowBrowserControlsToken);
        }
        if (mTabStripTransitionCoordinator != null
                && mTabStripTransitionToken == TokenHolder.INVALID_TOKEN) {
            mTabStripTransitionToken =
                    mTabStripTransitionCoordinator.requestDeferTabStripTransitionToken();
        }
    }

    protected void allowBrowserControlsHide() {
        if (mBrowserControlsVisibilityDelegate != null) {
            mBrowserControlsVisibilityDelegate.releasePersistentShowingToken(
                    mShowBrowserControlsToken);
            mShowBrowserControlsToken = TokenHolder.INVALID_TOKEN;
        }
        if (mTabStripTransitionCoordinator != null) {
            mTabStripTransitionCoordinator.releaseTabStripToken(mTabStripTransitionToken);
            mTabStripTransitionToken = TokenHolder.INVALID_TOKEN;
        }
    }

    /** Set the {@link TabStripTransitionCoordinator} that manages interactions around tab strip. */
    public void setTabStripTransitionCoordinator(TabStripTransitionCoordinator coordinator) {
        mTabStripTransitionCoordinator = coordinator;
    }

    /**
     * Notify the observer that the toolbar color is changed and pass the toolbar color to the
     * observer.
     */
    protected void notifyToolbarColorChanged(@ColorInt int color) {
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
        if (shadow != null) {
            shadow.setVisibility(isHairlineVisible ? VISIBLE : GONE);
        }
    }

    /**
     * To be called indirectly by
     * {@link LayoutStateProvider.LayoutStateObserver#onStartedHiding(int, boolean, boolean)}.
     */
    public void onTransitionStart() {}

    /**
     * To be called indirectly by
     * {@link LayoutStateProvider.LayoutStateObserver#onFinishedShowing(int)}.
     */
    public void onTransitionEnd() {}

    /**
     * Called when the home button is pressed. Will record the home button action if the NTP is
     * visible. Used on both phones and tablets.
     */
    protected void recordHomeModuleClickedIfNTPVisible() {
        if (getToolbarDataProvider().getNewTabPageDelegate().isCurrentlyVisible()) {
            // Record the clicking action on the home button.
            BrowserUiUtils.recordModuleClickHistogram(ModuleTypeOnStartAndNtp.HOME_BUTTON);
        }
    }

    /** Requests keyboard focus on the toolbar row. */
    public abstract void requestKeyboardFocus();
}
