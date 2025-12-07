// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.chrome.browser.toolbar.top.ToolbarUtils.isToolbarTabletResizeRefactorEnabled;
import static org.chromium.ui.accessibility.KeyboardFocusUtil.setFocusOnFirstFocusableDescendant;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewStub;
import android.view.accessibility.AccessibilityEvent;
import android.widget.ImageButton;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.layouts.toolbar.ToolbarWidthConsumer;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.toolbar.back_button.BackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionToolbarCoordinator;
import org.chromium.chrome.browser.toolbar.forward_button.ForwardButtonCoordinator;
import org.chromium.chrome.browser.toolbar.home_button.HomeButtonCoordinator;
import org.chromium.chrome.browser.toolbar.incognito.IncognitoIndicatorCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.reload_button.ReloadButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup.HistoryDelegate;
import org.chromium.chrome.browser.toolbar.top.ToolbarUtils.ToolbarComponentId;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.function.Supplier;

/** The Toolbar object for Tablet screens. */
@SuppressLint("Instantiatable")
@NullMarked
public class ToolbarTablet extends ToolbarLayout {
    private static final int MINIMUM_LOCATION_BAR_WIDTH_DP = 200;

    private ImageButton mHomeButton;
    private ImageButton mBackButton;
    private ImageButton mReloadButton;
    private ImageButton mBookmarkButton;

    private boolean mIsInTabSwitcherMode;
    private boolean mToolbarButtonsVisible;
    private boolean mOptionalButtonForciblyHidden;
    private @Nullable ImageButton mOptionalButton;
    private boolean mOptionalButtonUsesTint;

    private @Nullable Boolean mIsIncognitoBranded;
    private LocationBarCoordinator mLocationBar;
    private ReloadButtonCoordinator mReloadButtonCoordinator;
    private BackButtonCoordinator mBackButtonCoordinator;
    private IncognitoIndicatorCoordinator mIncognitoIndicatorCoordinator;
    private ForwardButtonCoordinator mForwardButtonCoordinator;

    private final int mStartPaddingWithButtons;
    private final int mStartPaddingWithoutButtons;
    private boolean mShouldAnimateButtonVisibilityChange;
    private @Nullable AnimatorSet mButtonVisibilityAnimators;
    private @Nullable ObservableSupplier<Integer> mTabCountSupplier;
    private @Nullable TabletCaptureStateToken mLastCaptureStateToken;
    private @DrawableRes int mBookmarkButtonImageRes;
    private @Nullable ExtensionToolbarCoordinator mExtensionToolbarCoordinator;

    private final @Nullable ToolbarWidthConsumer[] mToolbarWidthConsumers =
            new ToolbarWidthConsumer[ToolbarComponentId.COUNT];

    /**
     * Constructs a ToolbarTablet object.
     *
     * @param context The Context in which this View object is created.
     * @param attrs The AttributeSet that was specified with this View.
     */
    public ToolbarTablet(Context context, AttributeSet attrs) {
        super(context, attrs);
        mStartPaddingWithButtons =
                getResources().getDimensionPixelOffset(R.dimen.tablet_toolbar_start_padding);
        mStartPaddingWithoutButtons =
                getResources().getDimensionPixelOffset(R.dimen.toolbar_edge_padding);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        mHomeButton = findViewById(R.id.home_button);
        mBackButton = findViewById(R.id.back_button);
        mReloadButton = findViewById(R.id.refresh_button);

        mBookmarkButton = findViewById(R.id.bookmark_button);

        // Initialize values needed for showing/hiding toolbar buttons when the activity size
        // changes.
        mShouldAnimateButtonVisibilityChange = false;
        mToolbarButtonsVisible = true;
    }

    @Override
    @Initializer
    public void setLocationBarCoordinator(LocationBarCoordinator locationBarCoordinator) {
        mLocationBar = locationBarCoordinator;
        final @ColorInt int color = SemanticColorUtils.getColorSurfaceContainer(getContext());
        mLocationBar.getTabletCoordinator().tintBackground(color);

        mToolbarWidthConsumers[ToolbarComponentId.OMNIBOX_BOOKMARK] =
                mLocationBar.getBookmarkButtonToolbarWidthConsumer();
        mToolbarWidthConsumers[ToolbarComponentId.OMNIBOX_ZOOM] =
                mLocationBar.getZoomButtonToolbarWidthConsumer();
        mToolbarWidthConsumers[ToolbarComponentId.OMNIBOX_INSTALL] =
                mLocationBar.getInstallButtonToolbarWidthConsumer();
        mToolbarWidthConsumers[ToolbarComponentId.OMNIBOX_MIC] =
                mLocationBar.getMicButtonToolbarWidthConsumer();
        mToolbarWidthConsumers[ToolbarComponentId.OMNIBOX_LENS] =
                mLocationBar.getLensButtonToolbarWidthConsumer();
    }

    @Override
    public boolean showContextMenuForChild(View originalView) {
        if (mForwardButtonCoordinator != null
                && mForwardButtonCoordinator.getButton() == originalView) {
            // Display forwards navigation popup.
            mForwardButtonCoordinator.displayNavigationPopup();
            return true;
        }
        return super.showContextMenuForChild(originalView);
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        mForwardButtonCoordinator.onWindowFocusChanged(hasWindowFocus);
        super.onWindowFocusChanged(hasWindowFocus);
    }

    @Override
    public void setTextureCaptureMode(boolean textureMode) {
        if (textureMode) {
            mLastCaptureStateToken = generateCaptureStateToken();
        }
    }

    @Override
    public CaptureReadinessResult isReadyForTextureCapture() {
        if (urlHasFocus()) {
            return CaptureReadinessResult.notReady(TopToolbarBlockCaptureReason.URL_BAR_HAS_FOCUS);
        } else if (mIsInTabSwitcherMode) {
            return CaptureReadinessResult.notReady(TopToolbarBlockCaptureReason.TAB_SWITCHER_MODE);
        } else if (mButtonVisibilityAnimators != null) {
            return CaptureReadinessResult.notReady(
                    TopToolbarBlockCaptureReason.TABLET_BUTTON_ANIMATION_IN_PROGRESS);
        } else {
            return getReadinessStateWithSuppression();
        }
    }

    private CaptureReadinessResult getReadinessStateWithSuppression() {
        TabletCaptureStateToken currentToken = generateCaptureStateToken();
        final @ToolbarSnapshotDifference int difference =
                currentToken.getAnyDifference(mLastCaptureStateToken);
        if (difference == ToolbarSnapshotDifference.NONE) {
            return CaptureReadinessResult.notReady(TopToolbarBlockCaptureReason.SNAPSHOT_SAME);
        } else {
            return CaptureReadinessResult.readyWithSnapshotDifference(difference);
        }
    }

    private TabletCaptureStateToken generateCaptureStateToken() {
        UrlBarData urlBarData;
        final @DrawableRes int securityIconResource;

        urlBarData = mLocationBar.getUrlBarData();
        if (urlBarData == null) urlBarData = getToolbarDataProvider().getUrlBarData();
        StatusCoordinator statusCoordinator = mLocationBar.getStatusCoordinator();
        securityIconResource =
                statusCoordinator == null
                        ? getToolbarDataProvider().getSecurityIconResource(false)
                        : statusCoordinator.getSecurityIconResource();

        VisibleUrlText visibleUrlText =
                new VisibleUrlText(
                        urlBarData.displayText, mLocationBar.getOmniboxVisibleTextPrefixHint());
        int tabCount = mTabCountSupplier == null ? 0 : mTabCountSupplier.get();

        return new TabletCaptureStateToken(
                mHomeButton,
                mBackButton,
                mForwardButtonCoordinator != null ? mForwardButtonCoordinator.getButton() : null,
                mReloadButton,
                securityIconResource,
                visibleUrlText,
                mBookmarkButton,
                mBookmarkButtonImageRes,
                mOptionalButton,
                tabCount,
                getWidth());
    }

    @Override
    void onTabOrModelChanged() {
        super.onTabOrModelChanged();
        final boolean incognitoBranded = isIncognitoBranded();
        if (mIsIncognitoBranded == null || mIsIncognitoBranded != incognitoBranded) {
            // TODO (amaralp): Have progress bar observe theme color and incognito changes directly.
            getProgressBar()
                    .setThemeColor(
                            ChromeColors.getDefaultThemeColor(getContext(), incognitoBranded),
                            incognitoBranded);
            updateRippleBackground();
            mIsIncognitoBranded = incognitoBranded;
        }
        updateNtp();
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        ImageViewCompat.setImageTintList(mHomeButton, activityFocusTint);
        if (mOptionalButton != null && mOptionalButtonUsesTint) {
            ImageViewCompat.setImageTintList(mOptionalButton, activityFocusTint);
        }
    }

    @Override
    public void onThemeColorChanged(@ColorInt int color, boolean shouldAnimate) {
        setBackgroundColor(color);
        final @ColorInt int textBoxColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        getContext(), color, isIncognitoBranded(), /* isCustomTab= */ false);
        mLocationBar.getTabletCoordinator().tintBackground(textBoxColor);
        mLocationBar.updateVisualsForState();
        setToolbarHairlineColor(color);

        // Notify the StatusBarColorController of the toolbar color change. This is to match the
        // status bar's color with the toolbar color when the tab strip is hidden on a tablet.
        notifyToolbarColorChanged(color);
    }

    /** Called when the currently visible New Tab Page changes. */
    private void updateNtp() {
        NewTabPageDelegate ntpDelegate = getToolbarDataProvider().getNewTabPageDelegate();
        ntpDelegate.setSearchBoxScrollListener(
                (scrollFraction) -> {
                    // Fade the search box out in the first 40% of the scrolling transition.
                    float alpha = Math.max(1f - scrollFraction * 2.5f, 0f);
                    ntpDelegate.setSearchBoxAlpha(alpha);
                    ntpDelegate.setSearchProviderLogoAlpha(alpha);
                });
    }

    /** Called when the tab model changes. */
    private void updateRippleBackground() {
        var toolbarIconRippleId = ToolbarUtils.getToolbarIconRippleId(isIncognitoBranded());
        var omniboxIconRippleId =
                isIncognitoBranded()
                        ? R.drawable.search_box_icon_background_baseline
                        : R.drawable.search_box_icon_background;

        mHomeButton.setBackgroundResource(toolbarIconRippleId);
        getMenuButtonCoordinator().updateButtonBackground(toolbarIconRippleId);

        mBookmarkButton.setBackgroundResource(omniboxIconRippleId);
        mLocationBar.updateButtonBackground(omniboxIconRippleId);

        if (mExtensionToolbarCoordinator != null) {
            mExtensionToolbarCoordinator.updateMenuButtonBackground(toolbarIconRippleId);
        }
    }

    @Override
    void onTabContentViewChanged() {
        super.onTabContentViewChanged();
        updateNtp();
    }

    @Override
    void updateButtonVisibility() {
        mLocationBar.updateButtonVisibility();
        mForwardButtonCoordinator.updateEnabled();
    }

    @Override
    void updateBookmarkButton(boolean isBookmarked, boolean editingAllowed) {
        if (isBookmarked) {
            mBookmarkButtonImageRes = R.drawable.btn_star_filled;
            mBookmarkButton.setImageResource(R.drawable.btn_star_filled);
            final @ColorRes int tint =
                    isIncognitoBranded()
                            ? R.color.default_icon_color_blue_light
                            : R.color.default_icon_color_accent1_tint_list;
            ImageViewCompat.setImageTintList(
                    mBookmarkButton, AppCompatResources.getColorStateList(getContext(), tint));
            mBookmarkButton.setContentDescription(getContext().getString(R.string.edit_bookmark));
        } else {
            mBookmarkButtonImageRes = R.drawable.star_outline_24dp;
            mBookmarkButton.setImageResource(R.drawable.star_outline_24dp);
            ImageViewCompat.setImageTintList(mBookmarkButton, getTint());
            mBookmarkButton.setContentDescription(
                    getContext().getString(R.string.accessibility_menu_bookmark));
        }
        mBookmarkButton.setEnabled(editingAllowed);
    }

    @Override
    void setTabSwitcherMode(boolean inTabSwitcherMode) {
        mIsInTabSwitcherMode = inTabSwitcherMode;
        int importantForAccessibility =
                inTabSwitcherMode
                        ? View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                        : View.IMPORTANT_FOR_ACCESSIBILITY_AUTO;

        mLocationBar.setUrlBarFocusable(!mIsInTabSwitcherMode);
        if (getImportantForAccessibility() != importantForAccessibility) {
            setImportantForAccessibility(importantForAccessibility);
            sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
        }
    }

    @Override
    @Initializer
    public void initialize(
            ToolbarDataProvider toolbarDataProvider,
            ToolbarTabController tabController,
            MenuButtonCoordinator menuButtonCoordinator,
            @Nullable ToggleTabStackButtonCoordinator tabSwitcherButtonCoordinator,
            HistoryDelegate historyDelegate,
            UserEducationHelper userEducationHelper,
            ObservableSupplier<Tracker> trackerSupplier,
            ToolbarProgressBar progressBar,
            @Nullable ReloadButtonCoordinator reloadButtonCoordinator,
            @Nullable BackButtonCoordinator backButtonCoordinator,
            @Nullable ForwardButtonCoordinator forwardButtonCoordinator,
            @Nullable HomeButtonDisplay homeButtonDisplay,
            @Nullable ExtensionToolbarCoordinator extensionToolbarCoordinator,
            ThemeColorProvider themeColorProvider,
            IncognitoStateProvider incognitoStateProvider,
            @Nullable Supplier<Integer> incognitoWindowCountSupplier) {
        assert tabSwitcherButtonCoordinator != null;
        super.initialize(
                toolbarDataProvider,
                tabController,
                menuButtonCoordinator,
                tabSwitcherButtonCoordinator,
                historyDelegate,
                userEducationHelper,
                trackerSupplier,
                progressBar,
                reloadButtonCoordinator,
                backButtonCoordinator,
                forwardButtonCoordinator,
                homeButtonDisplay,
                extensionToolbarCoordinator,
                themeColorProvider,
                incognitoStateProvider,
                incognitoWindowCountSupplier);
        mReloadButtonCoordinator = assertNonNull(reloadButtonCoordinator);
        mBackButtonCoordinator = assertNonNull(backButtonCoordinator);
        mForwardButtonCoordinator = assertNonNull(forwardButtonCoordinator);
        menuButtonCoordinator.setVisibility(true);
        mExtensionToolbarCoordinator = extensionToolbarCoordinator;

        assert incognitoWindowCountSupplier != null;
        mIncognitoIndicatorCoordinator =
                new IncognitoIndicatorCoordinator(
                        /* parentToolbar= */ this,
                        themeColorProvider,
                        incognitoStateProvider,
                        incognitoWindowCountSupplier,
                        mToolbarButtonsVisible);

        if (homeButtonDisplay instanceof ToolbarWidthConsumer) {
            mToolbarWidthConsumers[ToolbarComponentId.HOME] =
                    (HomeButtonCoordinator) homeButtonDisplay;
        }
        mToolbarWidthConsumers[ToolbarComponentId.BACK] = mBackButtonCoordinator;
        mToolbarWidthConsumers[ToolbarComponentId.FORWARD] = mForwardButtonCoordinator;
        mToolbarWidthConsumers[ToolbarComponentId.RELOAD] = mReloadButtonCoordinator;
        mToolbarWidthConsumers[ToolbarComponentId.LOCATION_BAR_MINIMUM] =
                new LocationBarMinWidthConsumer();
        mToolbarWidthConsumers[ToolbarComponentId.INCOGNITO_INDICATOR] =
                mIncognitoIndicatorCoordinator;
        mToolbarWidthConsumers[ToolbarComponentId.ADAPTIVE_BUTTON] =
                new OptionalButtonToolbarWidthConsumer();
        mToolbarWidthConsumers[ToolbarComponentId.TAB_SWITCHER] = tabSwitcherButtonCoordinator;
        mToolbarWidthConsumers[ToolbarComponentId.MENU] = menuButtonCoordinator;
        mToolbarWidthConsumers[ToolbarComponentId.PADDING] =
                new ToolbarPaddingWidthConsumer(this, mStartPaddingWithButtons);
    }

    @Override
    public void destroy() {
        super.destroy();
        if (mButtonVisibilityAnimators != null) {
            mButtonVisibilityAnimators.removeAllListeners();
            mButtonVisibilityAnimators.cancel();
            mButtonVisibilityAnimators = null;
        }
    }

    @Override
    void setTabCountSupplier(ObservableSupplier<Integer> tabCountSupplier) {
        mTabCountSupplier = tabCountSupplier;
    }

    @Override
    void setBookmarkClickHandler(@Nullable OnClickListener listener) {
        assert listener != null;
        mLocationBar.setBookmarkClickListener(listener);
    }

    @Override
    void onHomeButtonIsEnabledUpdate(boolean homeButtonEnabled) {
        mHomeButton.setVisibility(homeButtonEnabled ? VISIBLE : GONE);
    }

    @Override
    public LocationBar getLocationBar() {
        return mLocationBar;
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        // After the first layout, button visibility changes should be animated. On the first
        // layout, the button visibility shouldn't be animated because the visibility may be
        // changing solely because Chrome was launched into multi-window.
        mShouldAnimateButtonVisibilityChange = true;

        super.onLayout(changed, left, top, right, bottom);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = MeasureSpec.getSize(widthMeasureSpec);
        if (isToolbarTabletResizeRefactorEnabled()) {
            allocateAvailableToolbarWidth(mToolbarWidthConsumers, width);
        } else {
            // Hide or show toolbar buttons if needed. With the introduction of multi-window on
            // Android N, the Activity can be < 600dp, in which case the toolbar buttons need to be
            // moved into the menu so that the location bar is usable. The buttons must be shown
            // in onMeasure() so that the location bar gets measured and laid out correctly.
            setToolbarButtonsVisible(
                    width >= DeviceFormFactor.getNonMultiDisplayMinimumTabletWidthPx(getContext()));
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        // Trigger a second update if the incognito indicator was measured at a different width than
        // originally expected, requiring another pass at allocating toolbar width.
        // TODO(crbug.com/444068280): Revisit this approach to re-allocating width for variable
        //  width components.
        if (isToolbarTabletResizeRefactorEnabled()
                && mIncognitoIndicatorCoordinator.needsUpdateBeforeShowing()) {
            allocateAvailableToolbarWidth(mToolbarWidthConsumers, width);
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    @VisibleForTesting
    static void allocateAvailableToolbarWidth(
            @Nullable ToolbarWidthConsumer[] toolbarWidthConsumer, int availableWidthDp) {
        // Iterate through the toolbar components, which will show if there is enough available
        // width.
        for (@ToolbarComponentId int toolbarComponentId : ToolbarUtils.RANKED_TOOLBAR_COMPONENTS) {
            @Nullable ToolbarWidthConsumer widthConsumer = toolbarWidthConsumer[toolbarComponentId];
            if (widthConsumer == null) continue;
            availableWidthDp -= widthConsumer.updateVisibility(availableWidthDp);
        }
    }

    @Override
    protected void updateOptionalButton(ButtonData buttonData) {
        if (mOptionalButton == null) {
            ViewStub viewStub = findViewById(R.id.optional_button_stub);
            mOptionalButton = (ImageButton) viewStub.inflate();
        }

        ButtonSpec buttonSpec = buttonData.getButtonSpec();

        mOptionalButton.setBackgroundResource(
                isIncognitoBranded()
                        ? R.drawable.default_icon_background_baseline
                        : R.drawable.default_icon_background);

        // Set hover tooltip text for voice search, share and new tab button on tablets.
        if (buttonSpec.getHoverTooltipTextId() != ButtonSpec.INVALID_TOOLTIP_TEXT_ID) {
            super.setTooltipText(
                    mOptionalButton, getContext().getString(buttonSpec.getHoverTooltipTextId()));
        } else {
            super.setTooltipText(mOptionalButton, null);
        }

        mOptionalButtonUsesTint = buttonSpec.getSupportsTinting();
        if (mOptionalButtonUsesTint) {
            ImageViewCompat.setImageTintList(mOptionalButton, getTint());
        } else {
            ImageViewCompat.setImageTintList(mOptionalButton, null);
        }

        if (buttonSpec.getIphCommandBuilder() != null) {
            buttonSpec.getIphCommandBuilder().setAnchorView(mOptionalButton);
        }
        mOptionalButton.setOnClickListener(buttonSpec.getOnClickListener());
        if (buttonSpec.getOnLongClickListener() == null) {
            mOptionalButton.setLongClickable(false);
        } else {
            mOptionalButton.setLongClickable(true);
            mOptionalButton.setOnLongClickListener(buttonSpec.getOnLongClickListener());
        }
        mOptionalButton.setImageDrawable(buttonSpec.getDrawable());

        // Adjusting the paddings ensures the avatar remains stationary while the error badge is
        // added or removed.
        int paddingStart =
                getDimensionPixelSize(
                        buttonSpec.hasErrorBadge()
                                ? R.dimen
                                        .optional_toolbar_tablet_button_with_error_badge_padding_start
                                : R.dimen.optional_toolbar_tablet_button_padding_start);
        int paddingTop =
                getDimensionPixelSize(
                        buttonSpec.hasErrorBadge()
                                ? R.dimen
                                        .optional_toolbar_tablet_button_with_error_badge_padding_top
                                : R.dimen.optional_toolbar_tablet_button_padding_top);
        mOptionalButton.setPaddingRelative(
                paddingStart,
                paddingTop,
                mOptionalButton.getPaddingEnd(),
                mOptionalButton.getPaddingBottom());

        mOptionalButton.setContentDescription(buttonSpec.getContentDescription());
        mOptionalButtonForciblyHidden = false;
        setOptionalButtonVisibility(/* isVisible= */ true);
        mOptionalButton.setEnabled(buttonData.isEnabled());
    }

    @Override
    protected void hideOptionalButton() {
        mOptionalButtonForciblyHidden = true;
        setOptionalButtonVisibility(/* isVisible= */ false);
    }

    private void setOptionalButtonVisibility(boolean isVisible) {
        if (mOptionalButton == null) return;
        mOptionalButton.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    private class ToolbarPaddingWidthConsumer implements ToolbarWidthConsumer {
        private final View mToolbarView;
        private final int mHorizontalPadding;

        ToolbarPaddingWidthConsumer(View toolbarView, int horizontalPadding) {
            mToolbarView = toolbarView;
            mHorizontalPadding = horizontalPadding;
        }

        @Override
        public boolean isVisible() {
            return mToolbarView.getPaddingStart() == mHorizontalPadding
                    && mToolbarView.getPaddingEnd() == mHorizontalPadding;
        }

        @Override
        public int updateVisibility(int availableWidth) {
            assert availableWidth >= 0;
            int paddingWidth = Math.min(availableWidth, 2 * mHorizontalPadding);
            mToolbarView.setPaddingRelative(
                    paddingWidth / 2, getPaddingTop(), paddingWidth / 2, getPaddingBottom());
            return paddingWidth;
        }

        @Override
        public int updateVisibilityWithAnimation(
                int availableWidth, Collection<Animator> animators) {
            return updateVisibility(availableWidth);
        }
    }

    private class LocationBarMinWidthConsumer implements ToolbarWidthConsumer {
        @Override
        public boolean isVisible() {
            return true;
        }

        @Override
        public int updateVisibility(int availableWidth) {
            assert isToolbarTabletResizeRefactorEnabled();
            return Math.min(
                    availableWidth,
                    (int)
                            (MINIMUM_LOCATION_BAR_WIDTH_DP
                                    * getContext().getResources().getDisplayMetrics().density));
        }

        @Override
        public int updateVisibilityWithAnimation(
                int availableWidth, Collection<Animator> animators) {
            return updateVisibility(availableWidth);
        }
    }

    private class OptionalButtonToolbarWidthConsumer implements ToolbarWidthConsumer {
        @Override
        public boolean isVisible() {
            return mOptionalButton != null && mOptionalButton.getVisibility() == View.VISIBLE;
        }

        @Override
        public int updateVisibility(int availableWidth) {
            assert isToolbarTabletResizeRefactorEnabled();
            if (mOptionalButtonForciblyHidden) {
                setOptionalButtonVisibility(false);
                return 0;
            }

            int width = getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);
            if (availableWidth >= width) {
                setOptionalButtonVisibility(true);
                return width;
            } else {
                setOptionalButtonVisibility(false);
                return 0;
            }
        }

        @Override
        public int updateVisibilityWithAnimation(
                int availableWidth, Collection<Animator> animators) {
            return updateVisibility(availableWidth);
        }
    }

    @Override
    public @Nullable View getOptionalButtonViewForTesting() {
        return mOptionalButton;
    }

    @Override
    public void requestKeyboardFocus() {
        setFocusOnFirstFocusableDescendant(this);
        // TODO(crbug.com/360423850): Replace this setFocus(mLocationBar) when omnibox keyboard
        // behavior is fixed.
    }

    private void setToolbarButtonsVisible(boolean visible) {
        if (mToolbarButtonsVisible == visible) return;

        mToolbarButtonsVisible = visible;

        if (mShouldAnimateButtonVisibilityChange) {
            runToolbarButtonsVisibilityAnimation(visible);
        } else {
            mForwardButtonCoordinator.setVisibility(visible);
            mReloadButtonCoordinator.setVisibility(visible);
            mBackButtonCoordinator.setVisibility(visible);
            mLocationBar.setShouldShowButtonsWhenUnfocusedForTablet(visible);
            setStartPaddingBasedOnButtonVisibility(visible);
            mIncognitoIndicatorCoordinator.setVisibility(visible);
        }
    }

    /**
     * Sets the toolbar start padding based on whether the buttons are visible.
     *
     * @param buttonsVisible Whether the toolbar buttons are visible.
     */
    private void setStartPaddingBasedOnButtonVisibility(boolean buttonsVisible) {
        buttonsVisible = buttonsVisible || mHomeButton.getVisibility() == View.VISIBLE;

        this.setPaddingRelative(
                buttonsVisible ? mStartPaddingWithButtons : mStartPaddingWithoutButtons,
                getPaddingTop(),
                ViewCompat.getPaddingEnd(this),
                getPaddingBottom());
    }

    /**
     * @return The difference in start padding when the buttons are visible and when they are not
     * visible.
     */
    public int getStartPaddingDifferenceForButtonVisibilityAnimation() {
        // If the home button is visible then the padding doesn't change.
        return mHomeButton.getVisibility() == View.VISIBLE
                ? 0
                : mStartPaddingWithButtons - mStartPaddingWithoutButtons;
    }

    /** Returns whether tab switcher mode is enabled. */
    public boolean isInTabSwitcherMode() {
        return mIsInTabSwitcherMode;
    }

    private void runToolbarButtonsVisibilityAnimation(boolean visible) {
        if (mButtonVisibilityAnimators != null) mButtonVisibilityAnimators.cancel();

        Collection<Animator> animators = new ArrayList<>();
        animators.add(mForwardButtonCoordinator.getFadeAnimator(visible));
        animators.add(mReloadButtonCoordinator.getFadeAnimator(visible));
        animators.add(mBackButtonCoordinator.getFadeAnimator(visible));
        animators.addAll(createLocationBarButtonsWhenUnfocusedAnimators(visible));

        mButtonVisibilityAnimators = new AnimatorSet();
        mButtonVisibilityAnimators.playTogether(animators);
        mButtonVisibilityAnimators.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onStart(Animator animator) {
                        keepControlsShownForAnimation();
                        if (visible) {
                            mForwardButtonCoordinator.setVisibility(true);
                            mReloadButtonCoordinator.setVisibility(true);
                            mBackButtonCoordinator.setVisibility(true);
                            mIncognitoIndicatorCoordinator.setVisibility(true);
                            // Set the padding at the start of the show animation so the toolbar
                            // buttons don't jump when the animation ends.
                            setStartPaddingBasedOnButtonVisibility(true);
                        }
                    }

                    @Override
                    public void onCancel(Animator animator) {
                        mButtonVisibilityAnimators = null;
                        allowBrowserControlsHide();
                    }

                    @Override
                    public void onEnd(Animator animator) {
                        if (!visible) {
                            mForwardButtonCoordinator.setVisibility(false);
                            mReloadButtonCoordinator.setVisibility(false);
                            mBackButtonCoordinator.setVisibility(false);
                            mIncognitoIndicatorCoordinator.setVisibility(false);
                            // Set the padding at the end of the hide animation so the toolbar
                            // buttons don't jump when the animation starts.
                            setStartPaddingBasedOnButtonVisibility(false);
                        }
                        mButtonVisibilityAnimators = null;
                        allowBrowserControlsHide();
                    }
                });
        mButtonVisibilityAnimators.start();
    }

    private List<Animator> createLocationBarButtonsWhenUnfocusedAnimators(boolean shouldShow) {
        int startPaddingDifference = getStartPaddingDifferenceForButtonVisibilityAnimation();
        return shouldShow
                ? mLocationBar.getShowButtonsWhenUnfocusedAnimatorsForTablet(startPaddingDifference)
                : mLocationBar.getHideButtonsWhenUnfocusedAnimatorsForTablet(
                        startPaddingDifference);
    }

    private int getDimensionPixelSize(@DimenRes int dimenId) {
        return getResources().getDimensionPixelSize(dimenId);
    }

    void enableButtonVisibilityChangeAnimationForTesting() {
        mShouldAnimateButtonVisibilityChange = true;
    }

    void setToolbarButtonsVisibleForTesting(boolean value) {
        mToolbarButtonsVisible = value;
    }

    @VisibleForTesting
    void setReloadButtonCoordinator(ReloadButtonCoordinator coordinator) {
        mReloadButtonCoordinator = coordinator;
        mToolbarWidthConsumers[ToolbarComponentId.RELOAD] = mReloadButtonCoordinator;
    }

    @VisibleForTesting
    void setBackButtonCoordinator(BackButtonCoordinator coordinator) {
        mBackButtonCoordinator = coordinator;
        mToolbarWidthConsumers[ToolbarComponentId.BACK] = mBackButtonCoordinator;
    }

    void setHomeButtonWidthConsumerForTesting(ToolbarWidthConsumer consumer) {
        mToolbarWidthConsumers[ToolbarComponentId.HOME] = consumer;
    }

    void setIncognitoIndicatorCoordinatorForTesting(IncognitoIndicatorCoordinator coordinator) {
        mIncognitoIndicatorCoordinator = coordinator;
        mToolbarWidthConsumers[ToolbarComponentId.INCOGNITO_INDICATOR] = coordinator;
    }

    void setForwardButtonCoordinatorForTesting(ForwardButtonCoordinator coordinator) {
        mForwardButtonCoordinator = coordinator;
        mToolbarWidthConsumers[ToolbarComponentId.FORWARD] = mForwardButtonCoordinator;
    }

    void ensureOptionalButtonWidthConsumerForTesting() {
        mToolbarWidthConsumers[ToolbarComponentId.ADAPTIVE_BUTTON] =
                new OptionalButtonToolbarWidthConsumer();
    }

    void setTabStackButtonCoordinatorForTesting(ToggleTabStackButtonCoordinator coordinator) {
        mToolbarWidthConsumers[ToolbarComponentId.TAB_SWITCHER] = coordinator;
    }

    @Override
    void setMenuButtonCoordinatorForTesting(MenuButtonCoordinator coordinator) {
        mMenuButtonCoordinator = coordinator;
        mToolbarWidthConsumers[ToolbarComponentId.MENU] = coordinator;
    }

    void ensurePaddingWidthConsumer() {
        mToolbarWidthConsumers[ToolbarComponentId.PADDING] =
                new ToolbarPaddingWidthConsumer(this, mStartPaddingWithButtons);
    }

    void ensureLocationBarMidWidthConsumer() {
        mToolbarWidthConsumers[ToolbarComponentId.LOCATION_BAR_MINIMUM] =
                new LocationBarMinWidthConsumer();
    }

    public boolean areAnyToolbarComponentsMissingForWidth(
            @ToolbarComponentId int[] toolbarComponents) {
        for (@ToolbarComponentId int toolbarComponentId : toolbarComponents) {
            @Nullable ToolbarWidthConsumer widthConsumer =
                    mToolbarWidthConsumers[toolbarComponentId];
            if (widthConsumer == null || !widthConsumer.isVisible()) return true;
        }
        return false;
    }
}
